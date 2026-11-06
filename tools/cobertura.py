"""Repete os testes com instrumentação e conserva os relatórios mesmo se falharem."""

import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

RAIZ = Path(__file__).resolve().parents[1]


def executar(argumentos, env=None):
    print("+ " + " ".join(map(str, argumentos)), flush=True)
    return subprocess.run(list(map(str, argumentos)), cwd=RAIZ, env=env).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--etapa", choices=["00", "01", "02", "03"], required=True)
    args = parser.parse_args()
    for modulo in ["coverage", "gcovr"]:
        if importlib.util.find_spec(modulo) is None:
            parser.error("Instale requirements-dev.txt com o Python usado no make.")
    if not shutil.which("gcov"):
        parser.error("Instale GCC/g++ e gcov da mesma versão para medir C++.")
    instrumentado = RAIZ / "build/cobertura"
    relatorios = RAIZ / "build/relatorios"
    # Somente saídas geradas; uma medição não pode herdar contagens da anterior.
    for destino in [instrumentado, relatorios]:
        if destino.exists():
            shutil.rmtree(destino)
        destino.mkdir(parents=True)
    for linguagem in ["cpp", "python"]:
        (relatorios / linguagem).mkdir()
    flags = "-std=c++17 -Wall -Wextra -Werror -pedantic -O0 -g --coverage"
    resultado = executar(["make", "build", "BUILD_DIR=build/cobertura", f"CXXFLAGS={flags}"])
    if resultado:
        return resultado
    env = dict(os.environ, COVERAGE_FILE=str(instrumentado / ".coverage"))
    def cobertura(*argumentos):
        return executar([sys.executable, "-m", "coverage", argumentos[0],
                         "--rcfile", str(RAIZ / ".coveragerc"), *argumentos[1:]], env)
    resultado = cobertura("run", "tools/testar.py", "--etapa", args.etapa,
                          "--controlador", "build/cobertura/controlador",
                          "--cpp-tests", "build/cobertura/autorais")
    erros = [cobertura("combine"),
             cobertura("report"),
             cobertura("html", "-d", "build/relatorios/python"),
             cobertura("json", "-o", "build/relatorios/python/resumo.json")]
    erros.append(executar([
        sys.executable, "-m", "gcovr", "--root", ".", "--filter", "controlador/",
        "--exclude", "controlador/(vendor|third_party)/", "build/cobertura",
        "--html-details", "build/relatorios/cpp/index.html",
        "--json-summary", "build/relatorios/cpp/resumo.json", "--print-summary",
    ]))
    resumo = ["# Evidências de cobertura", "",
              f"Etapa: {args.etapa}. Testes: {'FALHARAM' if resultado else 'PASSARAM'}.", "",
              "C++: cpp/index.html · Python: python/index.html.", "",
              "Leia os arquivos autorais separadamente da planta fornecida.",
              "Linhas e ramificações executadas não provam correção dos resultados."]
    (relatorios / "LEIA-ME.md").write_text("\n".join(resumo) + "\n", encoding="utf-8")
    print("Relatórios: build/relatorios/cpp/index.html e build/relatorios/python/index.html", flush=True)
    return 1 if resultado or any(erros) else 0


if __name__ == "__main__":
    sys.exit(main())
