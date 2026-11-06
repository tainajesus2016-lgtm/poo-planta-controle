import tempfile
from pathlib import Path
import unittest

from planta.simulador import Planta


class TestPlantaSemComando(unittest.TestCase):
    def test_primeiro_ciclo_mantem_atuadores_zerados(self):
        with tempfile.TemporaryDirectory() as diretorio:
            planta = Planta(seed=7, sessao="teste-do-exemplo")
            estado = planta.passo(Path(diretorio) / "comando_ausente.json")
        self.assertEqual(estado["ciclo"], 1)
        self.assertEqual(estado["atuadores"], {"bomba": 0, "aquecedor": 0})
        self.assertEqual(estado["ultimo_comando"], 0)


if __name__ == "__main__":
    unittest.main()
