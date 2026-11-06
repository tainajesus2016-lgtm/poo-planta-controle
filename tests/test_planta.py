import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from planta.simulador import Planta, gravar_json


class TestPlanta(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dados = Path(self.tmp.name)
        self.caminho = self.dados / "comando_planta.json"
        self.planta = Planta(seed=7, sessao="teste")

    def comando(self, **alteracoes):
        return dict(versao=1, sessao="teste", id=1,
                    atuadores={"bomba": 100, "aquecedor": 100}, **alteracoes)

    def test_sem_comando_produz_leituras_e_atuacao_segura(self):
        estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["ciclo"], 1)
        self.assertEqual(estado["atuadores"], {"bomba": 0, "aquecedor": 0})
        self.assertEqual(len(estado["leituras"]), 2)
        self.assertEqual(estado["ultimo_comando"], 0)

    def test_atuacao_altera_proximas_leituras(self):
        sem = Planta(seed=7, sessao="teste").passo(self.dados / "ausente")
        gravar_json(self.caminho, self.comando())
        com = self.planta.passo(self.caminho)
        for i in range(2):
            self.assertGreater(com["leituras"][i]["valor"], sem["leituras"][i]["valor"])
        self.assertEqual(com["ultimo_comando"], 1)

    def test_seed_repete_dinamica(self):
        outra = Planta(seed=7, sessao="teste")
        for _ in range(4):
            a = self.planta.passo(self.caminho)
            b = outra.passo(self.caminho)
            self.assertEqual(a["leituras"], b["leituras"])

    def test_releitura_nao_renova_comando(self):
        gravar_json(self.caminho, self.comando())
        for _ in range(5):
            estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["atuadores"]["bomba"], 100)
        estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["motivo_seguro"], "comando_expirado")
        self.assertEqual(estado["atuadores"]["bomba"], 0)
        novo = self.comando()
        novo["id"] = 2
        gravar_json(self.caminho, novo)
        estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["atuadores"]["bomba"], 100)
        self.assertIsNone(estado["motivo_seguro"])

    def test_comandos_invalidos_ficam_seguros(self):
        casos = [None, [], {}, {**self.comando(), "sessao": "outra"},
                 {**self.comando(), "versao": True},
                 {**self.comando(), "id": True},
                 {**self.comando(), "id": -1},
                 {**self.comando(), "atuadores": {"bomba": True, "aquecedor": 0}},
                 {**self.comando(), "atuadores": {"bomba": 101, "aquecedor": 0}},
                 {**self.comando(), "atuadores": {"bomba": "100", "aquecedor": 0}},
                 {**self.comando(), "atuadores": {"bomba": 0}},
                 {**self.comando(), "atuadores": {"bomba": 0, "aquecedor": 0, "x": 1}}]
        for comando in casos:
            with self.subTest(comando=comando):
                gravar_json(self.caminho, comando)
                estado = self.planta.passo(self.caminho)
                self.assertIsNotNone(estado["erro_comando"])
                self.assertTrue(all(v == 0 for v in estado["atuadores"].values()))

    def test_truncamento_e_recuperacao(self):
        self.caminho.write_text('{"id":', encoding="utf-8")
        self.assertIsNotNone(self.planta.passo(self.caminho)["erro_comando"])
        gravar_json(self.caminho, self.comando())
        self.assertEqual(self.planta.passo(self.caminho)["ultimo_comando"], 1)

    def test_reinicio_rejeita_sessao_antiga(self):
        gravar_json(self.caminho, self.comando())
        reiniciada = Planta(sessao="nova")
        self.assertEqual(reiniciada.passo(self.caminho)["ultimo_comando"], 0)

    def test_id_regressivo_nao_substitui_estado(self):
        comando = self.comando()
        comando["id"] = 3
        gravar_json(self.caminho, comando)
        self.planta.passo(self.caminho)
        gravar_json(self.caminho, self.comando())
        estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["ultimo_comando"], 3)
        self.assertIsNotNone(estado["erro_comando"])

    def test_limites_e_json_finito(self):
        for _ in range(150):
            comando = self.comando()
            comando["id"] = self.planta.ciclo + 1
            gravar_json(self.caminho, comando)
            estado = self.planta.passo(self.caminho)
            json.dumps(estado, allow_nan=False)
            for leitura in estado["leituras"]:
                self.assertTrue(0 <= leitura["valor"] <= 100)

    def test_publicacao_substitui_snapshot_sem_lixo(self):
        destino = self.dados / "saida.json"
        gravar_json(destino, {"ciclo": 1})
        gravar_json(destino, {"ciclo": 2})
        self.assertEqual(json.loads(destino.read_text()), {"ciclo": 2})
        self.assertEqual(list(self.dados.glob("*.tmp")), [])

    def test_cli_gera_snapshot(self):
        processo = subprocess.run(
            [sys.executable, "-m", "planta.simulador", "--dados", str(self.dados),
             "--passos", "3", "--intervalo", "0"], capture_output=True, text=True,
            timeout=10, check=True,
        )
        self.assertIn("ciclo=3", processo.stdout)
        self.assertEqual(json.loads((self.dados / "planta.json").read_text())["ciclo"], 3)

    def test_comissionamento_manual(self):
        gravar_json(self.dados / "planta.json", self.planta.passo(self.caminho))
        subprocess.run(
            [sys.executable, "tools/enviar_comando.py", "--dados", str(self.dados),
             "--bomba", "100", "--aquecedor", "50"], check=True, capture_output=True,
            timeout=10,
        )
        estado = self.planta.passo(self.caminho)
        self.assertEqual(estado["atuadores"], {"bomba": 100, "aquecedor": 50})


if __name__ == "__main__":
    unittest.main()
