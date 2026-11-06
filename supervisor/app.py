"""Painel de supervisão; consome apenas a telemetria publicada pelo controlador."""

import os
import sys
from pathlib import Path

import streamlit as st

# Permite importar "supervisor.telemetria" ao rodar como script.
RAIZ = Path(__file__).resolve().parents[1]
if str(RAIZ) not in sys.path:
    sys.path.insert(0, str(RAIZ))

from supervisor.telemetria import (
    atualizar_historico,
    calcular_idade,
    carregar_telemetria,
    montar_alarmes,
    montar_grafico,
    montar_linhas,
    montar_series,
)

# Limite de obsolescência da tela, independente do timeout de atuação da planta
# (contado em ciclos). Acima deste valor a aquisição é destacada como antiga,
# mas continua legível para diagnóstico.
LIMITE_OBSOLESCENCIA_S = 10.0

# A tela relê o arquivo publicado pelo controlador; não há broker nem banco.
INTERVALO_ATUALIZACAO_S = 2


def formatar_idade(idade):
    if idade is None:
        return "desconhecida"
    if idade < 0:
        return f"{idade:.1f} s (futuro)"
    return f"{idade:.1f} s"


def mostrar_alarmes(alarmes):
    st.subheader("Alertas")
    if not alarmes:
        st.success("Nenhum alarme ativo.")
        return

    # Cards coloridos: crítico em vermelho, aviso em amarelo e informação em azul.
    for alarme in alarmes:
        codigo = alarme["Código"] or "ALARME"
        mensagem = alarme["Mensagem"] or "Sem descrição"
        severidade = alarme["Severidade"] or "info"
        texto = f"{codigo}: {mensagem}"
        if severidade == "critico":
            st.error(texto)
        elif severidade == "aviso":
            st.warning(texto)
        else:
            st.info(texto)

    with st.expander("Detalhes dos alarmes"):
        st.dataframe(alarmes, use_container_width=True)


@st.fragment(run_every=INTERVALO_ATUALIZACAO_S)
def painel(dados):
    caminho = dados / "telemetria.json"
    st.caption(
        f"Ingestão: {caminho} (somente telemetria.json) · "
        f"atualização automática a cada {INTERVALO_ATUALIZACAO_S} s"
    )

    telemetria, erro = carregar_telemetria(dados)
    if erro:
        st.warning(erro)
        return

    sessao = telemetria.get("sessao", "desconhecida")
    ciclo = telemetria.get("ciclo", "desconhecido")
    timestamp = telemetria.get("timestamp")
    idade = calcular_idade(timestamp)
    linhas = montar_linhas(telemetria)
    grafico = montar_grafico(telemetria)
    alarmes = montar_alarmes(telemetria)

    historico = atualizar_historico(
        st.session_state.get("historico", []),
        telemetria,
    )
    st.session_state["historico"] = historico

    st.subheader("Estado atual")
    colunas = st.columns(4)
    with colunas[0]:
        st.metric("Ciclo", ciclo)
    with colunas[1]:
        st.metric("Idade da aquisição", formatar_idade(idade))
    with colunas[2]:
        st.metric("Leituras", len(linhas))
    with colunas[3]:
        st.metric("Alertas ativos", len(alarmes))

    st.caption(
        f"Sessão: {sessao} | Aquisição: {timestamp or 'desconhecida'}"
    )

    if idade is not None and idade > LIMITE_OBSOLESCENCIA_S:
        st.warning(
            "Telemetria antiga: a última aquisição tem mais de "
            f"{LIMITE_OBSOLESCENCIA_S:.0f} s. Os valores abaixo não são uma "
            "medição nova."
        )

    if linhas:
        st.dataframe(linhas, use_container_width=True)
        if grafico:
            st.subheader("Leituras atuais")
            st.bar_chart(grafico, x="Tag", y="Valor")
            st.caption("Cada tag usa a unidade indicada na tabela de leituras.")
    else:
        st.warning("Telemetria sem leituras.")

    series, tags = montar_series(historico)
    st.subheader("Histórico da sessão")
    if tags and len(series) >= 2:
        st.line_chart(series, x="Ciclo", y=tags)
        st.caption(
            f"{len(series)} aquisições nesta sessão. O histórico é mantido "
            "somente na memória do supervisor e deduplica por sessão/ciclo."
        )
    elif tags:
        st.info(
            "Aguardando novas aquisições para desenhar o histórico. "
            "O gráfico cresce a cada ciclo novo recebido enquanto a página "
            "estiver aberta."
        )
    else:
        st.info("O histórico ainda não possui leituras numéricas.")

    mostrar_alarmes(alarmes)


def main():
    dados = Path(os.environ.get("POO_DADOS", "dados"))
    st.title("Supervisório da planta didática")
    painel(dados)


if __name__ == "__main__":
    main()
