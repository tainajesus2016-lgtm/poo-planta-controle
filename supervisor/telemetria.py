"""Leitura e preparação da telemetria, sem depender do Streamlit.

Somente ``telemetria.json`` é consumido: o supervisor não lê ``planta.json``
nem escreve comandos na planta.
"""

import json
import math
from datetime import datetime, timezone
from pathlib import Path


FORMATO_TIMESTAMP = "%Y-%m-%dT%H:%M:%SZ"

# Histórico mantido apenas na memória do supervisor. O contrato não exige
# persistência e o supervisor não escreve arquivos.
LIMITE_HISTORICO_PADRAO = 120


def _valor_numerico(valor):
    """Aceita somente números finitos, recusando bool e textos numéricos."""
    return type(valor) in (int, float) and math.isfinite(valor)


def carregar_telemetria(pasta):
    """Lê ``<pasta>/telemetria.json``.

    Devolve ``(dados, None)`` quando o arquivo é um objeto JSON válido e
    ``(None, mensagem)`` quando ele não existe, não pode ser lido, não é JSON
    válido ou não tem um objeto como raiz.
    """
    caminho = Path(pasta) / "telemetria.json"
    try:
        with caminho.open(encoding="utf-8") as arquivo:
            dados = json.load(arquivo)
    except FileNotFoundError:
        return None, (
            f"Telemetria não encontrada em {caminho}. "
            "Execute a planta e o controlador para gerar o arquivo."
        )
    except json.JSONDecodeError as erro:
        return None, f"JSON inválido em {caminho}: {erro.msg} (linha {erro.lineno})."
    except (OSError, UnicodeError) as erro:
        return None, f"Não foi possível ler {caminho}: {erro}"

    if not isinstance(dados, dict):
        return None, (
            f"Telemetria inválida em {caminho}: a raiz deve ser um objeto JSON."
        )
    return dados, None


def montar_linhas(telemetria):
    """Converte as leituras da telemetria em linhas para apresentação.

    Entradas que não sejam objetos são ignoradas para que um arquivo
    malformado não interrompa a tela.
    """
    if not isinstance(telemetria, dict):
        return []
    leituras = telemetria.get("leituras")
    if not isinstance(leituras, list):
        return []

    linhas = []
    for leitura in leituras:
        if not isinstance(leitura, dict):
            continue
        linhas.append(
            {
                "Tag": leitura.get("tag"),
                "Tipo": leitura.get("tipo"),
                "Valor": leitura.get("valor"),
                "Unidade": leitura.get("unidade"),
                "Status": leitura.get("status"),
            }
        )
    return linhas


def montar_grafico(telemetria):
    """Devolve pares Tag/Valor numéricos para o gráfico das leituras atuais.

    Leituras sem valor numérico finito, como ``valor: null`` em falha, ficam
    fora do gráfico para não distorcer a escala.
    """
    grafico = []
    for linha in montar_linhas(telemetria):
        tag = linha["Tag"]
        valor = linha["Valor"]
        if not isinstance(tag, str) or not tag:
            continue
        if type(valor) not in (int, float) or not math.isfinite(valor):
            continue
        grafico.append({"Tag": tag, "Valor": valor})
    return grafico


def atualizar_historico(historico, telemetria, limite=LIMITE_HISTORICO_PADRAO):
    """Acrescenta a aquisição atual ao histórico, sem duplicar sessão/ciclo.

    O histórico é uma lista de amostras por aquisição, mantida apenas em
    memória. Ao detectar uma nova sessão, as amostras anteriores são
    descartadas para não misturar ciclos de sessões diferentes.
    """
    if not isinstance(historico, list):
        historico = []

    if not isinstance(telemetria, dict):
        return list(historico)

    sessao = telemetria.get("sessao")
    ciclo = telemetria.get("ciclo")
    if not isinstance(sessao, str) or not sessao or type(ciclo) is not int:
        return list(historico)

    if historico:
        ultima = historico[-1]
        if isinstance(ultima, dict) and ultima.get("sessao") != sessao:
            historico = []

    for amostra in historico:
        if (
            isinstance(amostra, dict)
            and amostra.get("sessao") == sessao
            and amostra.get("ciclo") == ciclo
        ):
            return list(historico)

    valores = {}
    for linha in montar_linhas(telemetria):
        tag = linha["Tag"]
        valor = linha["Valor"]
        if isinstance(tag, str) and tag and _valor_numerico(valor):
            valores[tag] = valor

    amostra = {
        "sessao": sessao,
        "ciclo": ciclo,
        "timestamp": telemetria.get("timestamp"),
        "valores": valores,
    }
    atualizado = list(historico) + [amostra]
    if type(limite) is int and limite > 0:
        atualizado = atualizado[-limite:]
    return atualizado


def montar_series(historico):
    """Prepara o histórico para o gráfico de linhas.

    Devolve ``(linhas, tags)``: uma linha por aquisição, com o ciclo como eixo
    x e uma coluna numérica por tag encontrada no histórico.
    """
    if not isinstance(historico, list):
        return [], []

    amostras = [
        amostra
        for amostra in historico
        if isinstance(amostra, dict) and type(amostra.get("ciclo")) is int
    ]

    tags = []
    for amostra in amostras:
        valores = amostra.get("valores")
        if not isinstance(valores, dict):
            continue
        for tag, valor in valores.items():
            if (
                isinstance(tag, str)
                and tag
                and tag not in tags
                and _valor_numerico(valor)
            ):
                tags.append(tag)

    linhas = []
    for amostra in amostras:
        valores = amostra.get("valores")
        if not isinstance(valores, dict):
            valores = {}
        linha = {"Ciclo": amostra["ciclo"]}
        for tag in tags:
            valor = valores.get(tag)
            if _valor_numerico(valor):
                linha[tag] = valor
        linhas.append(linha)
    return linhas, tags


def calcular_idade(timestamp, agora=None):
    """Devolve a idade de ``timestamp`` em segundos, ou ``None`` se inválido.

    Aceita o formato UTC do contrato: ``AAAA-MM-DDTHH:MM:SSZ``. ``agora`` pode
    ser informado em testes; por padrão usa o instante UTC atual.
    """
    if not isinstance(timestamp, str):
        return None
    try:
        instante = datetime.strptime(timestamp, FORMATO_TIMESTAMP)
    except (TypeError, ValueError):
        return None
    instante = instante.replace(tzinfo=timezone.utc)

    if agora is None:
        agora = datetime.now(timezone.utc)
    elif not isinstance(agora, datetime):
        return None
    elif agora.tzinfo is None:
        agora = agora.replace(tzinfo=timezone.utc)
    else:
        agora = agora.astimezone(timezone.utc)

    return (agora - instante).total_seconds()


def montar_alarmes(telemetria):
    """Devolve os alarmes ativos em linhas prontas para apresentação."""
    if not isinstance(telemetria, dict):
        return []
    alarmes = telemetria.get("alarmes")
    if not isinstance(alarmes, list):
        return []

    linhas = []
    for alarme in alarmes:
        if not isinstance(alarme, dict):
            continue
        linhas.append(
            {
                "Código": alarme.get("codigo"),
                "Mensagem": alarme.get("mensagem"),
                "Severidade": alarme.get("severidade"),
            }
        )
    return linhas
