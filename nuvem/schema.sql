-- =====================================================================
-- NoiseTrack: banco na nuvem (Supabase)
-- Cole este arquivo inteiro no SQL Editor do Supabase e clique em "Run".
-- Pode rodar mais de uma vez: tudo usa "if not exists" ou "or replace".
--
-- Ideia geral:
--   * A PLACA nunca escreve direto nas tabelas. Ela chama a função
--     aparelho_sync() com o ID e a chave dela (papel "anon").
--   * O SITE (pessoa logada, papel "authenticated") só LÊ os próprios
--     aparelhos e leituras, e muda coisas chamando funções.
--   * O RLS (Row Level Security) garante que cada pessoa só enxerga as
--     linhas dela, mesmo que alguém tente pedir as dos outros.
-- =====================================================================

-- pgcrypto: dá as funções crypt() e gen_salt(), usadas para guardar a
-- chave do aparelho embaralhada (bcrypt), nunca em texto.
create extension if not exists pgcrypto with schema extensions;


-- ---------------------------------------------------------------------
-- Tabelas
-- ---------------------------------------------------------------------

-- Um aparelho NoiseTrack. Cadastrado à mão (ver README); o dono entra
-- quando alguém digita o código de pareamento no site.
create table if not exists public.aparelhos (
  id                text primary key,                          -- ex.: noisetrack-a1b2c3
  dono              uuid references auth.users(id) on delete set null,
  codigo_pareamento text unique not null,                      -- ex.: ABCD-EFGH (maiúsculas)
  chave_hash        text not null,                             -- bcrypt da chave da placa
  limite_db         int  not null default 65 check (limite_db between 30 and 100),
  segundos          int  not null default 5  check (segundos  between 1 and 60),
  nivel_db          numeric,                                   -- último nível enviado
  estado            text,                                      -- calmo | contando | alerta | sem_sinal
  visto_em          timestamptz,                               -- última vez que a placa falou
  criado_em         timestamptz default now()
);

-- Histórico de níveis (no máximo 1 linha a cada 10 s por aparelho).
create table if not exists public.leituras (
  id          bigint generated always as identity primary key,
  aparelho_id text references public.aparelhos(id) on delete cascade,
  nivel_db    numeric,
  estado      text,
  criado_em   timestamptz default now()
);

create index if not exists leituras_aparelho_criado_idx
  on public.leituras (aparelho_id, criado_em desc);


-- ---------------------------------------------------------------------
-- RLS: cada pessoa só vê o que é dela
-- ---------------------------------------------------------------------
alter table public.aparelhos enable row level security;
alter table public.leituras  enable row level security;

drop policy if exists "dono le seus aparelhos" on public.aparelhos;
create policy "dono le seus aparelhos"
  on public.aparelhos for select
  to authenticated
  using (dono = (select auth.uid()));

drop policy if exists "dono le leituras dos seus aparelhos" on public.leituras;
create policy "dono le leituras dos seus aparelhos"
  on public.leituras for select
  to authenticated
  using (
    exists (
      select 1 from public.aparelhos a
      where a.id = leituras.aparelho_id
        and a.dono = (select auth.uid())
    )
  );

-- Não há políticas de INSERT, UPDATE ou DELETE: com RLS ligado, isso já
-- bloqueia qualquer escrita direta. As escritas acontecem só dentro das
-- funções abaixo (security definer).


-- ---------------------------------------------------------------------
-- Permissões nas tabelas (o auto-grant do projeto está desligado)
-- ---------------------------------------------------------------------
revoke all on table public.aparelhos from public, anon, authenticated;
revoke all on table public.leituras  from public, anon, authenticated;

-- Pessoa logada: lê as colunas de aparelhos, MENOS chave_hash.
-- Por isso o site precisa listar as colunas no select (select('*') falha).
grant select (id, dono, codigo_pareamento, limite_db, segundos,
              nivel_db, estado, visto_em, criado_em)
  on public.aparelhos to authenticated;

grant select on public.leituras to authenticated;

-- "anon" (a placa e visitantes sem login) não recebe nada nas tabelas.


-- ---------------------------------------------------------------------
-- Funções
-- ---------------------------------------------------------------------

-- A placa chama esta função a cada poucos segundos.
-- Entrada: ID, chave em texto, nível em dB e estado.
-- Saída:   {"limite_db": 65, "segundos": 5} para a placa aplicar.
create or replace function public.aparelho_sync(
  p_id     text,
  p_chave  text,
  p_nivel  numeric,
  p_estado text
)
returns json
language plpgsql
security definer
set search_path = public, extensions
as $$
declare
  v_ap     public.aparelhos%rowtype;
  v_nivel  numeric;
  v_estado text;
begin
  -- "for update" trava a linha: duas chamadas ao mesmo tempo do mesmo
  -- aparelho esperam uma pela outra, e a regra de 10 s não falha.
  select * into v_ap from public.aparelhos where id = p_id for update;

  if not found then
    -- Faz uma conta bcrypt à toa para a resposta demorar o mesmo tanto
    -- que com um ID existente (não revela quais IDs existem).
    perform crypt(coalesce(p_chave, ''), gen_salt('bf'));
    raise exception 'aparelho_invalido';
  end if;

  if p_chave is null or v_ap.chave_hash <> crypt(p_chave, v_ap.chave_hash) then
    raise exception 'aparelho_invalido';
  end if;

  -- Nível limitado a 0–150 dB (nulo continua nulo).
  v_nivel := case when p_nivel is null then null
                  else least(greatest(p_nivel, 0), 150) end;

  -- Só os quatro estados conhecidos; qualquer outra coisa vira nulo.
  v_estado := case when p_estado in ('calmo', 'contando', 'alerta', 'sem_sinal')
                   then p_estado else null end;

  update public.aparelhos
     set nivel_db = v_nivel,
         estado   = v_estado,
         visto_em = now()
   where id = p_id;

  -- Histórico: no máximo 1 leitura a cada 10 s por aparelho.
  if not exists (
    select 1 from public.leituras
     where aparelho_id = p_id
       and criado_em > now() - interval '10 seconds'
  ) then
    insert into public.leituras (aparelho_id, nivel_db, estado)
    values (p_id, v_nivel, v_estado);
  end if;

  -- Limpeza: em ~1% das chamadas, apaga leituras com mais de 7 dias
  -- (de todos os aparelhos, para não sobrar lixo de quem saiu do ar).
  if random() < 0.01 then
    delete from public.leituras where criado_em < now() - interval '7 days';
  end if;

  return json_build_object('limite_db', v_ap.limite_db, 'segundos', v_ap.segundos);
end;
$$;


-- A pessoa logada digita o código da etiqueta e vira dona do aparelho.
create or replace function public.parear_aparelho(p_codigo text)
returns text
language plpgsql
security definer
set search_path = public, extensions
as $$
declare
  v_uid    uuid := auth.uid();
  v_codigo text := upper(trim(coalesce(p_codigo, '')));
  v_id     text;
begin
  if v_uid is null then
    raise exception 'nao_autenticado';
  end if;

  -- Já é meu? Devolve o ID (parear duas vezes não dá erro).
  select id into v_id
    from public.aparelhos
   where codigo_pareamento = v_codigo and dono = v_uid;
  if found then
    return v_id;
  end if;

  -- Só pega o aparelho se ele ainda não tiver dono.
  update public.aparelhos
     set dono = v_uid
   where codigo_pareamento = v_codigo
     and dono is null
  returning id into v_id;

  -- Código errado OU aparelho de outra pessoa: a mesma resposta, para não
  -- revelar quais códigos existem.
  if v_id is null then
    raise exception 'codigo_invalido';
  end if;

  return v_id;
end;
$$;


-- O dono muda o limite e os segundos pelo site.
create or replace function public.atualizar_config(
  p_id       text,
  p_limite   int,
  p_segundos int
)
returns void
language plpgsql
security definer
set search_path = public, extensions
as $$
declare
  v_uid uuid := auth.uid();
begin
  if v_uid is null then
    raise exception 'nao_autenticado';
  end if;
  if p_limite is null or p_limite not between 30 and 100 then
    raise exception 'limite_invalido';
  end if;
  if p_segundos is null or p_segundos not between 1 and 60 then
    raise exception 'segundos_invalido';
  end if;

  update public.aparelhos
     set limite_db = p_limite,
         segundos  = p_segundos
   where id = p_id
     and dono = v_uid;

  if not found then
    raise exception 'aparelho_nao_encontrado';
  end if;
end;
$$;


-- O dono solta o aparelho (ele volta a poder ser pareado com o código).
-- As leituras antigas são apagadas, para o próximo dono não ver o
-- histórico de quem usou antes.
create or replace function public.desparear_aparelho(p_id text)
returns void
language plpgsql
security definer
set search_path = public, extensions
as $$
declare
  v_uid uuid := auth.uid();
begin
  if v_uid is null then
    raise exception 'nao_autenticado';
  end if;

  update public.aparelhos
     set dono = null
   where id = p_id
     and dono = v_uid;

  if not found then
    raise exception 'aparelho_nao_encontrado';
  end if;

  delete from public.leituras where aparelho_id = p_id;
end;
$$;


-- ---------------------------------------------------------------------
-- Quem pode executar cada função
-- ---------------------------------------------------------------------
-- Primeiro tira de todo mundo (o Postgres dá EXECUTE a PUBLIC por padrão)...
revoke execute on function public.aparelho_sync(text, text, numeric, text) from public, anon, authenticated;
revoke execute on function public.parear_aparelho(text)                   from public, anon, authenticated;
revoke execute on function public.atualizar_config(text, int, int)        from public, anon, authenticated;
revoke execute on function public.desparear_aparelho(text)                from public, anon, authenticated;

-- ...e depois dá só para quem precisa.
grant execute on function public.aparelho_sync(text, text, numeric, text) to anon;           -- a placa
grant execute on function public.parear_aparelho(text)                   to authenticated;  -- o site
grant execute on function public.atualizar_config(text, int, int)        to authenticated;
grant execute on function public.desparear_aparelho(text)                to authenticated;
