import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from planta.simulador import Planta, gravar_json

APP = Path(__file__).resolve().parents[1] / "supervisor" / "app.py"

class TestSupervisor(unittest.TestCase):
    def test_apresenta_leitura_do_controlador(self):
        from streamlit.testing.v1 import AppTest

        with tempfile.TemporaryDirectory() as diretorio:
            dados = Path(diretorio)
            estado = Planta(sessao="tela-teste").passo(dados / "ausente")
            estado["leituras"][0]["valor"] = 63.25
            estado["alarmes"] = []
            gravar_json(dados / "telemetria.json", estado)
            with patch.dict(os.environ, {"POO_DADOS": diretorio}):
                tela = AppTest.from_file(str(APP)).run(timeout=20)
            self.assertFalse(tela.exception, "O supervisor falhou ao receber telemetria valida.")
            self.assertTrue(tela.dataframe, "Implemente uma tabela de leituras da telemetria.")
            conteudo = " ".join(tabela.value.to_string() for tabela in tela.dataframe)
            self.assertIn("63.25", conteudo, "A tabela precisa apresentar o valor recebido, sem usar amostra fixa.")
            self.assertIn("LT-01", conteudo)

    def test_ausencia_e_json_invalido_sao_visiveis(self):
        from streamlit.testing.v1 import AppTest

        with tempfile.TemporaryDirectory() as diretorio:
            for entrada in [None, "{"]:
                if entrada is not None:
                    (Path(diretorio) / "telemetria.json").write_text(entrada)
                with patch.dict(os.environ, {"POO_DADOS": diretorio}):
                    tela = AppTest.from_file(str(APP)).run(timeout=20)
                self.assertFalse(tela.exception)
                self.assertTrue(tela.warning or tela.error,
                                "Mostre aviso ou erro quando a telemetria nao puder ser lida.")
