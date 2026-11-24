// NoiseTrack: lógica do site.
//
// Regras de segurança seguidas aqui:
//  * Dados do banco só entram na página com textContent (nunca innerHTML).
//  * O site nunca grava direto nas tabelas: só LÊ (select) e chama as
//    funções do banco (rpc). Quem decide o que pode é o banco (RLS + funções).
(function () {
  "use strict";

  // ---------- Constantes ----------
  var ESCALA_MIN = 20;            // medidor e gráfico: 20 a 110 dB
  var ESCALA_MAX = 110;
  // A placa envia a cada 1 s (NUVEM_INTERVALO_MS no firmware). Com 10 s de folga,
  // um ou outro envio perdido não derruba o selo "Online".
  // Se o aparelho for configurado para enviar a cada 10 s, suba ONLINE_MS para 30 s
  // (senão ele aparece Offline entre um envio e outro).
  var ONLINE_MS = 10 * 1000;      // visto nos últimos 10 s = Online
  var ATUALIZAR_MS = 1 * 1000;    // cartões a cada 1 s (pausa com a aba em segundo plano)
  var GRAFICO_MS = 10 * 1000;     // gráfico a cada 10 s
  var JANELA_GRAFICO_MS = 30 * 60 * 1000;
  var BURACO_MS = 60 * 1000;      // sem leitura por mais de 1 min: a linha do gráfico é interrompida
  var NS_SVG = "http://www.w3.org/2000/svg";

  // Colunas que o site pode ler (chave_hash NÃO está aqui e nem é permitida).
  var COLUNAS_APARELHO = "id,limite_db,segundos,nivel_db,estado,visto_em,criado_em";

  var NOMES_ESTADO = {
    calmo: "Calmo",
    contando: "Contando",
    alerta: "Alerta",
    sem_sinal: "Sem sinal",
    offline: "Offline"
  };

  // Mensagens amigáveis para os erros que as funções do banco levantam.
  var ERROS = {
    codigo_invalido: "Código não encontrado, ou o aparelho já está pareado com outra conta. Confira a etiqueta.",
    nao_autenticado: "Sua sessão terminou. Entre de novo.",
    limite_invalido: "O limite deve ser um número inteiro entre 30 e 100 dB.",
    segundos_invalido: "Os segundos devem ser um número inteiro entre 1 e 60.",
    aparelho_nao_encontrado: "Este aparelho não está mais na sua conta. Recarregue a página."
  };

  function el(id) { return document.getElementById(id); }

  function mostrarMensagem(alvo, texto, tipo) {
    alvo.textContent = texto || "";
    if (tipo) alvo.setAttribute("data-tipo", tipo);
    else alvo.removeAttribute("data-tipo");
  }

  // Traduz um erro do supabase-js para uma frase simples.
  function textoDoErro(erro) {
    if (!erro) return "Algo deu errado. Tente de novo.";
    var msg = String(erro.message || erro);
    for (var chave in ERROS) {
      if (Object.prototype.hasOwnProperty.call(ERROS, chave) && msg.indexOf(chave) >= 0) return ERROS[chave];
    }
    if (/Failed to fetch|NetworkError|network/i.test(msg)) return "Sem conexão com o servidor. Confira a internet.";
    if (/JWT|expired/i.test(msg)) return ERROS.nao_autenticado;
    return "Algo deu errado. Tente de novo.";
  }

  function textoDoErroLogin(erro) {
    var codigo = erro && erro.code;
    var msg = String((erro && erro.message) || "");
    if (codigo === "invalid_credentials" || /Invalid login credentials/i.test(msg)) return "E-mail ou senha incorretos.";
    if (codigo === "email_not_confirmed" || /Email not confirmed/i.test(msg)) return "Confirme o seu e-mail antes de entrar (veja a caixa de entrada e o spam).";
    if (codigo === "user_already_exists" || /already registered/i.test(msg)) return "Já existe uma conta com este e-mail. Use Entrar.";
    if (codigo === "weak_password" || /Password should be/i.test(msg)) return "Senha fraca: use pelo menos 6 caracteres.";
    if (/rate limit|too many/i.test(msg)) return "Muitas tentativas. Espere um pouco e tente de novo.";
    if (/Failed to fetch|NetworkError/i.test(msg)) return "Sem conexão com o servidor. Confira a internet.";
    return "Não foi possível concluir. Tente de novo.";
  }

  function formatarDb(valor) {
    return Number(valor).toLocaleString("pt-BR", { minimumFractionDigits: 1, maximumFractionDigits: 1 });
  }

  function fracaoEscala(db) {
    return Math.min(1, Math.max(0, (db - ESCALA_MIN) / (ESCALA_MAX - ESCALA_MIN)));
  }

  function tempoDesde(ms) {
    var s = Math.max(0, Math.round(ms / 1000));
    if (s < 60) return "há " + s + " s";
    var m = Math.round(s / 60);
    if (m < 60) return "há " + m + " min";
    var h = Math.round(m / 60);
    if (h < 48) return "há " + h + " h";
    return "há " + Math.round(h / 24) + " dias";
  }

  // ---------- Configuração ----------
  var cfg = window.NOISETRACK_CONFIG || {};

  function configPreenchida() {
    var url = String(cfg.url || "");
    var chave = String(cfg.chave || "");
    return url !== "" && chave !== "" &&
      url.indexOf("COLE_AQUI") < 0 && chave.indexOf("COLE_AQUI") < 0 &&
      /^https:\/\//.test(url);
  }

  if (!configPreenchida()) {
    el("avisoConfig").hidden = false;
    return;
  }
  if (!window.supabase || typeof window.supabase.createClient !== "function") {
    el("avisoConfigTexto").textContent =
      "A biblioteca do Supabase não carregou. Confira a internet e recarregue a página. " +
      "Se continuar, confira também os valores do config.js.";
    el("avisoConfig").hidden = false;
    return;
  }

  var sb = window.supabase.createClient(cfg.url, cfg.chave);

  // ---------- Telas ----------
  var usuarioAtual = null;

  function mostrarTela(nome) {
    el("telaEntrada").hidden = nome !== "entrada";
    el("telaPainel").hidden = nome !== "painel";
    el("sessao").hidden = nome !== "painel";
  }

  function aoMudarSessao(sessao) {
    var usuario = sessao ? sessao.user : null;
    var mudou = (usuario && usuario.id) !== (usuarioAtual && usuarioAtual.id);
    usuarioAtual = usuario;
    if (usuario) {
      el("sessaoEmail").textContent = usuario.email || "";
      mostrarTela("painel");
      if (mudou) iniciarPainel();
    } else {
      pararPainel();
      mostrarTela("entrada");
    }
  }

  // ---------- Entrar, criar conta, sair ----------
  var formEntrada = el("formEntrada");
  var msgEntrada = el("mensagemEntrada");

  function lerEntrada() {
    var email = el("campoEmail").value.trim();
    var senha = el("campoSenha").value;
    if (!email || email.indexOf("@") < 1) { mostrarMensagem(msgEntrada, "Digite um e-mail válido.", "erro"); return null; }
    if (senha.length < 6) { mostrarMensagem(msgEntrada, "A senha precisa de pelo menos 6 caracteres.", "erro"); return null; }
    return { email: email, password: senha };
  }

  function ocupado(form, sim) {
    var botoes = form.querySelectorAll("button");
    for (var i = 0; i < botoes.length; i++) botoes[i].disabled = sim;
  }

  formEntrada.addEventListener("submit", function (ev) {
    ev.preventDefault();
    var dados = lerEntrada();
    if (!dados) return;
    ocupado(formEntrada, true);
    mostrarMensagem(msgEntrada, "Entrando…");
    sb.auth.signInWithPassword(dados).then(function (r) {
      if (r.error) mostrarMensagem(msgEntrada, textoDoErroLogin(r.error), "erro");
      else { mostrarMensagem(msgEntrada, ""); el("campoSenha").value = ""; }
    }).catch(function (e) {
      mostrarMensagem(msgEntrada, textoDoErroLogin(e), "erro");
    }).then(function () { ocupado(formEntrada, false); });
  });

  el("botaoCadastrar").addEventListener("click", function () {
    var dados = lerEntrada();
    if (!dados) return;
    ocupado(formEntrada, true);
    mostrarMensagem(msgEntrada, "Criando a conta…");
    dados.options = { emailRedirectTo: location.origin + location.pathname };
    sb.auth.signUp(dados).then(function (r) {
      if (r.error) {
        mostrarMensagem(msgEntrada, textoDoErroLogin(r.error), "erro");
      } else if (!r.data.session) {
        mostrarMensagem(msgEntrada, "Conta criada. Enviamos um e-mail de confirmação: abra o link e depois clique em Entrar.", "ok");
      } else {
        mostrarMensagem(msgEntrada, "");   // entrou direto (confirmação de e-mail desligada)
      }
    }).catch(function (e) {
      mostrarMensagem(msgEntrada, textoDoErroLogin(e), "erro");
    }).then(function () { ocupado(formEntrada, false); });
  });

  el("botaoSair").addEventListener("click", function () {
    sb.auth.signOut();
  });

  // ---------- Parear ----------
  var formParear = el("formParear");
  var msgParear = el("mensagemParear");

  // Aceita "k7mp3qxr", "K7MP 3QXR" ou "k7mp-3qxr" e manda "K7MP-3QXR".
  function normalizarCodigo(texto) {
    var limpo = String(texto).toUpperCase().replace(/[^A-Z0-9-]/g, "");
    if (/^[A-Z0-9]{8}$/.test(limpo)) limpo = limpo.slice(0, 4) + "-" + limpo.slice(4);
    return limpo;
  }

  formParear.addEventListener("submit", function (ev) {
    ev.preventDefault();
    var codigo = normalizarCodigo(el("campoCodigo").value);
    if (codigo.length < 4) { mostrarMensagem(msgParear, "Digite o código que está na etiqueta.", "erro"); return; }
    ocupado(formParear, true);
    mostrarMensagem(msgParear, "Pareando…");
    sb.rpc("parear_aparelho", { p_codigo: codigo }).then(function (r) {
      if (r.error) { mostrarMensagem(msgParear, textoDoErro(r.error), "erro"); return; }
      mostrarMensagem(msgParear, "Aparelho " + r.data + " pareado.", "ok");
      el("campoCodigo").value = "";
      atualizarAparelhos();
    }).catch(function (e) {
      mostrarMensagem(msgParear, textoDoErro(e), "erro");
    }).then(function () { ocupado(formParear, false); });
  });

  // ---------- Cartões ----------
  var cartoes = {};          // id do aparelho -> objeto com os elementos do cartão
  var timerCartoes = null, timerGrafico = null;
  var buscandoCartoes = false;
  var contadorCampos = 0;

  function criarCartao(id) {
    var raiz = el("modeloCartao").content.firstElementChild.cloneNode(true);
    var c = {
      id: id,
      raiz: raiz,
      nome: raiz.querySelector(".cartao-nome"),
      selo: raiz.querySelector(".selo"),
      seloTexto: raiz.querySelector(".selo-texto"),
      nivel: raiz.querySelector(".nivel-numero"),
      estado: raiz.querySelector(".estado-texto"),
      visto: raiz.querySelector(".visto"),
      medidor: raiz.querySelector(".medidor"),
      barra: raiz.querySelector(".medidor-barra"),
      marcaLimite: raiz.querySelector(".medidor-limite"),
      textoLimite: raiz.querySelector(".medidor-limite-texto"),
      svg: raiz.querySelector(".grafico-svg"),
      graficoVazio: raiz.querySelector(".grafico-vazio"),
      form: raiz.querySelector(".config"),
      campoLimite: raiz.querySelector(".campo-limite"),
      campoSegundos: raiz.querySelector(".campo-segundos"),
      msg: raiz.querySelector(".mensagem-config"),
      editando: false,       // a pessoa mexeu nos campos e ainda não salvou
      dados: null,
      buscandoGrafico: false
    };

    // Liga cada rótulo ao seu campo (ids únicos por cartão).
    contadorCampos++;
    var idLimite = "limite-" + contadorCampos, idSegundos = "segundos-" + contadorCampos;
    c.campoLimite.id = idLimite;
    c.campoSegundos.id = idSegundos;
    raiz.querySelector(".rotulo-limite").setAttribute("for", idLimite);
    raiz.querySelector(".rotulo-segundos").setAttribute("for", idSegundos);

    c.nome.textContent = id;

    function marcarEditando() {
      c.editando = true;
      mostrarMensagem(c.msg, "Alterações não salvas.");
    }
    c.campoLimite.addEventListener("input", marcarEditando);
    c.campoSegundos.addEventListener("input", marcarEditando);

    c.form.addEventListener("submit", function (ev) {
      ev.preventDefault();
      salvarConfig(c);
    });
    raiz.querySelector(".botao-desparear").addEventListener("click", function () {
      desparear(c);
    });

    el("listaAparelhos").appendChild(raiz);
    return c;
  }

  // O foco está num campo deste cartão? (não sobrescrever o que a pessoa digita)
  function focoNoCartao(c) {
    var ativo = document.activeElement;
    return ativo === c.campoLimite || ativo === c.campoSegundos;
  }

  function preencherCartao(c, ap) {
    c.dados = ap;
    var agora = Date.now();
    var visto = ap.visto_em ? Date.parse(ap.visto_em) : NaN;
    var online = !isNaN(visto) && agora - visto <= ONLINE_MS;
    var temNivel = ap.nivel_db !== null && ap.nivel_db !== undefined && !isNaN(Number(ap.nivel_db));

    var estado = online ? (NOMES_ESTADO[ap.estado] ? ap.estado : "") : "offline";
    c.raiz.setAttribute("data-estado", estado);

    c.selo.setAttribute("data-online", online ? "true" : "false");
    c.seloTexto.textContent = online ? "Online" : "Offline";

    c.nivel.textContent = temNivel ? formatarDb(ap.nivel_db) : "--";
    c.estado.textContent = estado ? NOMES_ESTADO[estado] : "Sem dados";

    if (isNaN(visto)) c.visto.textContent = "A placa ainda não enviou dados.";
    else if (online) c.visto.textContent = "Atualizado " + tempoDesde(agora - visto) + ".";
    else c.visto.textContent = "Último contato " + tempoDesde(agora - visto) + ". Valor pode estar desatualizado.";

    var nivel = temNivel ? Number(ap.nivel_db) : ESCALA_MIN;
    c.barra.style.width = (fracaoEscala(nivel) * 100) + "%";
    c.medidor.setAttribute("aria-valuenow", String(Math.round(Math.min(ESCALA_MAX, Math.max(ESCALA_MIN, nivel)))));
    c.medidor.setAttribute("aria-valuetext",
      (temNivel ? formatarDb(ap.nivel_db) + " dB" : "sem leitura") + ", limite " + ap.limite_db + " dB");
    c.marcaLimite.style.left = (fracaoEscala(ap.limite_db) * 100) + "%";
    c.textoLimite.textContent = "limite " + ap.limite_db;

    // Só preenche os campos se a pessoa não estiver mexendo neles.
    if (!c.editando && !focoNoCartao(c)) {
      c.campoLimite.value = String(ap.limite_db);
      c.campoSegundos.value = String(ap.segundos);
    }
  }

  function atualizarAparelhos() {
    if (!usuarioAtual || buscandoCartoes) return;
    buscandoCartoes = true;
    sb.from("aparelhos").select(COLUNAS_APARELHO).order("criado_em", { ascending: true })
      .then(function (r) {
        if (r.error) return;    // falha passageira: tenta de novo no próximo ciclo (1 s)
        var lista = r.data || [];
        var vistos = {};
        for (var i = 0; i < lista.length; i++) {
          var ap = lista[i];
          vistos[ap.id] = true;
          var novo = !cartoes[ap.id];
          if (novo) cartoes[ap.id] = criarCartao(ap.id);
          preencherCartao(cartoes[ap.id], ap);
          if (novo) atualizarGrafico(cartoes[ap.id]);
        }
        for (var id in cartoes) {   // aparelhos que saíram da conta
          if (Object.prototype.hasOwnProperty.call(cartoes, id) && !vistos[id]) {
            cartoes[id].raiz.remove();
            delete cartoes[id];
          }
        }
        el("listaVazia").hidden = lista.length > 0;
      })
      .catch(function () { /* sem rede: tenta de novo no próximo ciclo */ })
      .then(function () { buscandoCartoes = false; });
  }

  // ---------- Salvar configuração ----------
  function lerInteiro(texto, min, max) {
    var t = String(texto).trim();
    if (!/^-?\d+$/.test(t)) return null;
    var n = Number(t);
    return n >= min && n <= max ? n : null;
  }

  function salvarConfig(c) {
    var limite = lerInteiro(c.campoLimite.value, 30, 100);
    var segundos = lerInteiro(c.campoSegundos.value, 1, 60);
    if (limite === null) { mostrarMensagem(c.msg, ERROS.limite_invalido, "erro"); c.campoLimite.focus(); return; }
    if (segundos === null) { mostrarMensagem(c.msg, ERROS.segundos_invalido, "erro"); c.campoSegundos.focus(); return; }

    ocupado(c.form, true);
    mostrarMensagem(c.msg, "Salvando…");
    sb.rpc("atualizar_config", { p_id: c.id, p_limite: limite, p_segundos: segundos }).then(function (r) {
      if (r.error) { mostrarMensagem(c.msg, textoDoErro(r.error), "erro"); return; }
      c.editando = false;
      mostrarMensagem(c.msg, "Salvo: " + limite + " dB por " + segundos + " s. A placa aplica em alguns segundos.", "ok");
      atualizarAparelhos();
    }).catch(function (e) {
      mostrarMensagem(c.msg, textoDoErro(e), "erro");
    }).then(function () { ocupado(c.form, false); });
  }

  // ---------- Desparear ----------
  function desparear(c) {
    var ok = window.confirm("Desparear o aparelho " + c.id + "?\n\n" +
      "Ele sai da sua conta e o histórico dele é apagado. " +
      "Para voltar, será preciso digitar o código da etiqueta de novo.");
    if (!ok) return;
    ocupado(c.form, true);
    sb.rpc("desparear_aparelho", { p_id: c.id }).then(function (r) {
      if (r.error) { mostrarMensagem(c.msg, textoDoErro(r.error), "erro"); return; }
      c.raiz.remove();
      delete cartoes[c.id];
      el("listaVazia").hidden = Object.keys(cartoes).length > 0;
      mostrarMensagem(msgParear, "Aparelho " + c.id + " despareado.", "ok");
    }).catch(function (e) {
      mostrarMensagem(c.msg, textoDoErro(e), "erro");
    }).then(function () { if (cartoes[c.id]) ocupado(c.form, false); });
  }

  // ---------- Gráfico (SVG feito à mão, sem biblioteca) ----------
  var LARG = 320, ALT = 120;   // mesmo tamanho do viewBox no index.html

  function svgEl(nome, atributos) {
    var e = document.createElementNS(NS_SVG, nome);
    for (var k in atributos) if (Object.prototype.hasOwnProperty.call(atributos, k)) e.setAttribute(k, String(atributos[k]));
    return e;
  }

  function yDe(db) { return ALT - fracaoEscala(db) * ALT; }

  function desenharGrafico(c, leituras) {
    var svg = c.svg;
    while (svg.firstChild) svg.removeChild(svg.firstChild);

    var fim = Date.now(), inicio = fim - JANELA_GRAFICO_MS;

    // Grade leve em 50 e 80 dB.
    [50, 80].forEach(function (db) {
      svg.appendChild(svgEl("line", { "class": "linha-grade", x1: 0, x2: LARG, y1: yDe(db), y2: yDe(db) }));
    });

    // Monta o caminho; um buraco grande entre leituras interrompe a linha.
    var d = "", anterior = null, pontos = 0;
    for (var i = 0; i < leituras.length; i++) {
      var l = leituras[i];
      var t = Date.parse(l.criado_em);
      if (isNaN(t) || l.nivel_db === null || isNaN(Number(l.nivel_db))) { anterior = null; continue; }
      var x = Math.max(0, Math.min(LARG, ((t - inicio) / JANELA_GRAFICO_MS) * LARG));
      var y = yDe(Number(l.nivel_db));
      var comando = (anterior === null || t - anterior > BURACO_MS) ? "M" : "L";
      d += comando + x.toFixed(1) + " " + y.toFixed(1) + " ";
      anterior = t;
      pontos++;
    }
    if (d) svg.appendChild(svgEl("path", { "class": "linha-nivel", d: d.trim() }));

    // Linha tracejada no limite.
    if (c.dados) {
      var yl = yDe(c.dados.limite_db);
      svg.appendChild(svgEl("line", { "class": "linha-limite", x1: 0, x2: LARG, y1: yl, y2: yl }));
    }

    c.graficoVazio.hidden = pontos > 0;
  }

  function atualizarGrafico(c) {
    if (!usuarioAtual || c.buscandoGrafico) return;
    c.buscandoGrafico = true;
    var desde = new Date(Date.now() - JANELA_GRAFICO_MS).toISOString();
    sb.from("leituras").select("nivel_db,criado_em")
      .eq("aparelho_id", c.id).gte("criado_em", desde)
      .order("criado_em", { ascending: true }).limit(1000)
      .then(function (r) {
        if (!r.error && cartoes[c.id] === c) desenharGrafico(c, r.data || []);
      })
      .catch(function () {})
      .then(function () { c.buscandoGrafico = false; });
  }

  function atualizarGraficos() {
    for (var id in cartoes) if (Object.prototype.hasOwnProperty.call(cartoes, id)) atualizarGrafico(cartoes[id]);
  }

  // ---------- Ciclo de atualização (polling) ----------
  function iniciarPainel() {
    pararPainel();
    atualizarAparelhos();
    timerCartoes = setInterval(function () { if (!document.hidden) atualizarAparelhos(); }, ATUALIZAR_MS);
    timerGrafico = setInterval(function () { if (!document.hidden) atualizarGraficos(); }, GRAFICO_MS);
  }

  function pararPainel() {
    clearInterval(timerCartoes);
    clearInterval(timerGrafico);
    timerCartoes = timerGrafico = null;
    for (var id in cartoes) if (Object.prototype.hasOwnProperty.call(cartoes, id)) cartoes[id].raiz.remove();
    cartoes = {};
    mostrarMensagem(msgParear, "");
    el("listaVazia").hidden = true;
  }

  // Voltou para a aba: atualiza na hora, sem esperar o próximo ciclo.
  document.addEventListener("visibilitychange", function () {
    if (!document.hidden && usuarioAtual) { atualizarAparelhos(); atualizarGraficos(); }
  });

  // ---------- Início ----------
  // Fica no fim do arquivo para tudo acima já estar pronto quando a sessão chegar.
  sb.auth.getSession().then(function (r) { aoMudarSessao(r.data && r.data.session); });
  sb.auth.onAuthStateChange(function (_evento, sessao) { aoMudarSessao(sessao); });
})();
