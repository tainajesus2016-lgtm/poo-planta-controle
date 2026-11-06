"""Mostra o caminho dos dados sem implementar a decisão do controlador."""

import argparse
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dados", type=Path, default=Path("dados"))
    parser.add_argument("--arquivo", choices=["planta.json", "telemetria.json"], default="planta.json")
    args = parser.parse_args()
    caminho = args.dados / args.arquivo
    try:
        with caminho.open(encoding="utf-8") as arquivo:
            estado = json.load(arquivo)
        if not isinstance(estado, dict) or not isinstance(estado.get("leituras"), list):
            raise ValueError("documento deve conter um objeto com a lista leituras")
        linhas = [f"Ciclo: {estado['ciclo']}"]
        for leitura in estado["leituras"]:
            linhas.append(f"{leitura['tag']}: {leitura['valor']} {leitura['unidade']}")
        linhas.append(f"Comando confirmado: {estado['ultimo_comando']}")
        print("\n".join(linhas))
    except (OSError, ValueError, KeyError, TypeError) as erro:
        print(f"Nao foi possivel inspecionar {caminho}: {erro}", file=sys.stderr)
        print("Gere os dados primeiro e confira o diretorio de trabalho.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
