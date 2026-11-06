"""Comissionamento manual da planta; não implementa controle automático."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from planta.simulador import gravar_json, numero


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dados", type=Path, default=Path("dados"))
    parser.add_argument("--bomba", type=float, required=True)
    parser.add_argument("--aquecedor", type=float, required=True)
    args = parser.parse_args()
    if not all(numero(v, 0, 100) for v in [args.bomba, args.aquecedor]):
        parser.error("atuacoes devem estar entre 0 e 100")
    with (args.dados / "planta.json").open(encoding="utf-8") as arquivo:
        planta = json.load(arquivo)
    comando = {
        "versao": 1, "sessao": planta["sessao"], "id": planta["ultimo_comando"] + 1,
        "atuadores": {"bomba": args.bomba, "aquecedor": args.aquecedor},
    }
    gravar_json(args.dados / "comando_planta.json", comando)
    print(json.dumps(comando, ensure_ascii=False))
    print("Aguarde o proximo ciclo e confira ultimo_comando em planta.json.")


if __name__ == "__main__":
    main()
