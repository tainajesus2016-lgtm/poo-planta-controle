import argparse
import os
from pathlib import Path
import subprocess
import sys
import unittest

RAIZ = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RAIZ))
sys.path.insert(0, str(RAIZ / "tests"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--etapa", choices=["00", "01", "02", "03"], required=True)
    parser.add_argument("--controlador", type=Path, default=Path("build/controlador"))
    parser.add_argument("--cpp-tests", type=Path, default=Path("build/autorais"))
    args = parser.parse_args()
    etapa = int(args.etapa)
    os.environ["POO_CONTROLADOR"] = str(args.controlador.resolve())
    fontes_cpp = sorted((RAIZ / "tests/cpp").rglob("autorais_*.cpp"))
    executaveis = [args.cpp_tests / p.relative_to(RAIZ / "tests/cpp").with_suffix("")
                   for p in fontes_cpp]
    falha_cpp = False
    for executavel in executaveis:
        print(f"AUTORAL C++: {executavel}", flush=True)
        try:
            processo = subprocess.run([str(executavel.resolve())], timeout=30)
            falha_cpp |= processo.returncode != 0
        except (OSError, subprocess.TimeoutExpired) as erro:
            print(f"FALHOU: {executavel}: {erro}", file=sys.stderr)
            falha_cpp = True
    if etapa >= 1 and not executaveis:
        print("Faltam testes dos seus objetos C++: crie tests/cpp/autorais_*.cpp. Consulte docs/TESTES.md.", file=sys.stderr)
        falha_cpp = True
    suite = unittest.TestSuite()
    suite.addTests(unittest.defaultTestLoader.discover(str(RAIZ / "tests"), pattern="test_planta.py"))
    if etapa >= 1:
        from test_controlador import suite as controlador
        suite.addTests(controlador(etapa))
    if etapa >= 3:
        suite.addTests(unittest.defaultTestLoader.discover(str(RAIZ / "tests"), pattern="test_supervisor.py"))
    # Casos de domínio escritos pela equipe são repetidos em todas as etapas.
    autorais = unittest.defaultTestLoader.discover(str(RAIZ / "tests"), pattern="autorais_*.py")
    if etapa >= 3 and autorais.countTestCases() == 0:
        print("Faltam testes dos seus objetos Python: crie tests/autorais_*.py. Consulte docs/TESTES.md.", file=sys.stderr)
        falha_cpp = True
    suite.addTests(autorais)
    resultado = unittest.TextTestRunner(verbosity=2).run(suite)
    aprovado = resultado.wasSuccessful() and not falha_cpp
    print(f"Resumo: {'PASSOU' if aprovado else 'FALHOU'}; "
          f"{len(executaveis)} executaveis autorais C++ e "
          f"{autorais.countTestCases()} casos autorais Python.", flush=True)
    return 0 if aprovado else 1


if __name__ == "__main__":
    sys.exit(main())
