import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from planta.simulador import Planta, gravar_json, numero


class TestAquisicao(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dados = Path(self.tmp.name)
        self.planta = Planta(sessao="contrato-teste")
        self.entrada = self.planta.passo(self.dados / "comando_planta.json")
        gravar_json(self.dados / "planta.json", self.entrada)

    def executar(self):
        return subprocess.run(
            [os.environ.get("POO_CONTROLADOR", str(Path("build/controlador").resolve())),
             "--dados", str(self.dados), "--uma-vez"],
            capture_output=True, text=True, timeout=10,
        )

    def carregar_saida(self, nome):
        caminho = self.dados / nome
        self.assertTrue(caminho.exists(), f"Falta {nome}: o esqueleto ainda nao implementa esta etapa.")
        return json.loads(caminho.read_text(encoding="utf-8"))

    def test_aquisicao_preserva_origem_e_grandezas(self):
        self.assertEqual(self.executar().returncode, 0)
        saida = self.carregar_saida("telemetria.json")
        for chave in ["versao", "sessao", "ciclo", "timestamp", "leituras", "atuadores", "ultimo_comando"]:
            self.assertEqual(saida.get(chave), self.entrada[chave], f"Campo {chave} perdeu o significado da origem.")
        self.assertIsInstance(saida.get("alarmes"), list, "Telemetria deve conter alarmes, mesmo vazios.")

    def test_entrada_invalida_nao_vira_telemetria_nova(self):
        for documento in ["{", "null", "[]", "{}"]:
            with self.subTest(documento=documento):
                gravar_json(self.dados / "telemetria.json", {"anterior": True})
                (self.dados / "planta.json").write_text(documento, encoding="utf-8")
                self.assertNotEqual(self.executar().returncode, 0, "Entrada invalida precisa ser sinalizada na CLI.")
                self.assertEqual(self.carregar_saida("telemetria.json"), {"anterior": True})

    def test_arquivo_ausente_sinaliza_falha(self):
        (self.dados / "planta.json").unlink()
        self.assertNotEqual(self.executar().returncode, 0, "Arquivo ausente nao e aquisicao bem-sucedida.")


class TestAtuacao(TestAquisicao):
    def test_comando_aplicado_no_proximo_ciclo(self):
        self.assertEqual(self.executar().returncode, 0)
        comando = self.carregar_saida("comando_planta.json")
        self.assertEqual(comando.get("sessao"), self.entrada["sessao"])
        self.assertIs(type(comando.get("id")), int)
        self.assertGreater(comando["id"], self.entrada["ultimo_comando"])
        self.assertEqual(set(comando.get("atuadores", {})), {"bomba", "aquecedor"})
        self.assertTrue(all(numero(v, 0, 100) for v in comando["atuadores"].values()))
        estado = self.planta.passo(self.dados / "comando_planta.json")
        self.assertIsNone(estado["erro_comando"], "A planta rejeitou o comando do controlador.")
        self.assertEqual(estado["ultimo_comando"], comando["id"])
        self.assertEqual(estado["atuadores"], comando["atuadores"])

    def test_mesmo_ciclo_nao_gera_novo_comando(self):
        self.executar()
        primeiro = self.carregar_saida("comando_planta.json")
        self.executar()
        self.assertEqual(self.carregar_saida("comando_planta.json"), primeiro,
                         "Reprocessar o mesmo ciclo nao deve renovar o comando.")


def suite(etapa):
    classe = TestAtuacao if etapa >= 2 else TestAquisicao
    return unittest.defaultTestLoader.loadTestsFromTestCase(classe)
