"""Casos autorais da lógica de leitura/apresentação do supervisor.

Não importa Streamlit para que estes testes também rodem nas etapas 01 e 02.
"""

import json
from datetime import datetime, timezone
from pathlib import Path
import tempfile
import unittest

from supervisor.telemetria import (
    atualizar_historico,
    calcular_idade,
    carregar_telemetria,
    montar_alarmes,
    montar_grafico,
    montar_linhas,
    montar_series,
)


TELEMETRIA = {
    "versao": 1,
    "sessao": "sessao-autoral",
    "ciclo": 4,
    "timestamp": "2026-10-21T18:00:00Z",
    "leituras": [
        {"tag": "LT-01", "tipo": "nivel", "valor": 63.25, "unidade": "%", "status": "operando"},
        {"tag": "TT-01", "tipo": "temperatura", "valor": 25.5, "unidade": "C", "status": "operando"},
    ],
    "atuadores": {"bomba": 0.0, "aquecedor": 0.0},
    "ultimo_comando": 0,
    "erro_comando": None,
    "motivo_seguro": "sem_arquivo",
    "alarmes": [],
}


class TestLeituraTelemetria(unittest.TestCase):
    def setUp(self):
        self.temporario = tempfile.TemporaryDirectory()
        self.pasta = Path(self.temporario.name)
        self.caminho = self.pasta / "telemetria.json"

    def tearDown(self):
        self.temporario.cleanup()

    def test_arquivo_ausente_devolve_erro_sem_excecao(self):
        dados, erro = carregar_telemetria(self.pasta)

        self.assertIsNone(dados)
        self.assertIn("não encontrada", erro)

    def test_json_invalido_devolve_erro_sem_excecao(self):
        self.caminho.write_text("{", encoding="utf-8")

        dados, erro = carregar_telemetria(self.pasta)

        self.assertIsNone(dados)
        self.assertIn("JSON inválido", erro)

    def test_raiz_que_nao_e_objeto_devolve_erro(self):
        for conteudo in ("[]", "null", '"texto"'):
            with self.subTest(conteudo=conteudo):
                self.caminho.write_text(conteudo, encoding="utf-8")

                dados, erro = carregar_telemetria(self.pasta)

                self.assertIsNone(dados)
                self.assertIn("objeto JSON", erro)

    def test_telemetria_valida_devolve_documento(self):
        self.caminho.write_text(json.dumps(TELEMETRIA), encoding="utf-8")

        dados, erro = carregar_telemetria(self.pasta)

        self.assertIsNone(erro)
        self.assertEqual(dados["sessao"], "sessao-autoral")
        self.assertEqual(dados["ciclo"], 4)


class TestApresentacaoTelemetria(unittest.TestCase):
    def _telemetria(self, ciclo, sessao="sessao-autoral", nivel=50.0):
        return {
            **TELEMETRIA,
            "sessao": sessao,
            "ciclo": ciclo,
            "leituras": [
                {"tag": "LT-01", "tipo": "nivel", "valor": nivel,
                 "unidade": "%", "status": "operando"},
                {"tag": "TT-01", "tipo": "temperatura", "valor": 25.0,
                 "unidade": "C", "status": "operando"},
            ],
        }

    def test_montar_linhas_preserva_campos_valor_e_tag(self):
        linhas = montar_linhas(TELEMETRIA)

        self.assertEqual(len(linhas), 2)
        self.assertEqual(
            set(linhas[0]), {"Tag", "Tipo", "Valor", "Unidade", "Status"}
        )
        self.assertEqual(linhas[0]["Tag"], "LT-01")
        self.assertEqual(linhas[0]["Valor"], 63.25)
        self.assertEqual(linhas[0]["Unidade"], "%")

    def test_montar_linhas_ignora_entradas_malformadas(self):
        telemetria = {
            "leituras": [
                "invalida",
                {"tag": "LT-01"},
                None,
            ]
        }

        linhas = montar_linhas(telemetria)

        self.assertEqual(len(linhas), 1)
        self.assertEqual(linhas[0]["Tag"], "LT-01")
        self.assertIsNone(linhas[0]["Valor"])

    def test_montar_linhas_sem_lista_devolve_vazio(self):
        for telemetria in ({}, {"leituras": None}, {"leituras": "texto"}):
            with self.subTest(telemetria=telemetria):
                self.assertEqual(montar_linhas(telemetria), [])

    def test_montar_grafico_preserva_tags_e_valores_numericos(self):
        grafico = montar_grafico(TELEMETRIA)

        self.assertEqual(
            grafico,
            [{"Tag": "LT-01", "Valor": 63.25}, {"Tag": "TT-01", "Valor": 25.5}],
        )

    def test_montar_grafico_ignora_valores_nao_numericos_ou_nao_finitos(self):
        telemetria = {
            "leituras": [
                {"tag": "LT-01", "valor": None, "status": "falha"},
                {"tag": "TT-01", "valor": True},
                {"tag": "TT-02", "valor": "25.0"},
                {"tag": "", "valor": 10.0},
                {"tag": "TT-03", "valor": float("nan")},
                {"tag": "TT-04", "valor": 30.0},
            ]
        }

        self.assertEqual(montar_grafico(telemetria), [{"Tag": "TT-04", "Valor": 30.0}])

    def test_historico_acrescenta_e_deduplica_por_sessao_e_ciclo(self):
        historico = atualizar_historico([], TELEMETRIA)
        historico = atualizar_historico(historico, TELEMETRIA)

        self.assertEqual(len(historico), 1)
        self.assertEqual(historico[0]["sessao"], "sessao-autoral")
        self.assertEqual(historico[0]["ciclo"], 4)
        self.assertEqual(historico[0]["valores"]["LT-01"], 63.25)

    def test_historico_limita_quantidade_de_amostras(self):
        historico = []
        for ciclo in range(1, 5):
            historico = atualizar_historico(
                historico, self._telemetria(ciclo), limite=2
            )

        self.assertEqual([amostra["ciclo"] for amostra in historico], [3, 4])

    def test_historico_descarta_amostras_de_sessao_anterior(self):
        historico = atualizar_historico([], self._telemetria(1))
        historico = atualizar_historico(
            historico, self._telemetria(1, sessao="outra-sessao")
        )

        self.assertEqual(len(historico), 1)
        self.assertEqual(historico[0]["sessao"], "outra-sessao")

    def test_historico_ignora_telemetria_invalida(self):
        anterior = atualizar_historico([], TELEMETRIA)

        self.assertEqual(atualizar_historico(anterior, "texto"), anterior)
        self.assertEqual(atualizar_historico(anterior, {"sessao": "x"}), anterior)

    def test_montar_series_prepara_ciclo_e_tags(self):
        historico = atualizar_historico([], self._telemetria(1, nivel=50.0))
        historico = atualizar_historico(historico, self._telemetria(2, nivel=55.0))

        series, tags = montar_series(historico)

        self.assertEqual(tags, ["LT-01", "TT-01"])
        self.assertEqual([linha["Ciclo"] for linha in series], [1, 2])
        self.assertEqual(series[0]["LT-01"], 50.0)
        self.assertEqual(series[1]["LT-01"], 55.0)

    def test_calcular_idade_com_agora_fixo(self):
        agora = datetime(2026, 10, 21, 18, 0, 10, tzinfo=timezone.utc)

        idade = calcular_idade("2026-10-21T18:00:00Z", agora)

        self.assertEqual(idade, 10.0)

    def test_calcular_idade_com_timestamp_invalido_devolve_none(self):
        for timestamp in (None, "", "21/10/2026 18:00:00", "2026-13-40T99:00:00Z"):
            with self.subTest(timestamp=timestamp):
                self.assertIsNone(calcular_idade(timestamp))

    def test_montar_alarmes_vazio_devolve_lista_vazia(self):
        self.assertEqual(montar_alarmes({"alarmes": []}), [])
        self.assertEqual(montar_alarmes({}), [])

    def test_montar_alarmes_preserva_codigo_mensagem_e_severidade(self):
        telemetria = {
            "alarmes": [
                {
                    "codigo": "NIVEL_BAIXO",
                    "mensagem": "Nivel abaixo da faixa definida",
                    "severidade": "aviso",
                }
            ]
        }

        alarmes = montar_alarmes(telemetria)

        self.assertEqual(len(alarmes), 1)
        self.assertEqual(
            alarmes[0],
            {
                "Código": "NIVEL_BAIXO",
                "Mensagem": "Nivel abaixo da faixa definida",
                "Severidade": "aviso",
            },
        )


if __name__ == "__main__":
    unittest.main()
