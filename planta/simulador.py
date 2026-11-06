"""Tanque hipotético: dinâmica discreta, perturbações e atuação por JSON local."""

import argparse
import json
import math
import os
from pathlib import Path
import random
import sys
import tempfile
import time
from datetime import datetime, timezone
from uuid import uuid4


def gravar_json(caminho, documento):
    """Publica um snapshot completo no mesmo sistema de arquivos."""
    caminho = Path(caminho)
    caminho.parent.mkdir(parents=True, exist_ok=True)
    temporario = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=caminho.parent,
            prefix=f".{caminho.name}.", suffix=".tmp", delete=False,
        ) as arquivo:
            temporario = Path(arquivo.name)
            json.dump(documento, arquivo, ensure_ascii=False, allow_nan=False)
            arquivo.write("\n")
        os.replace(temporario, caminho)
    finally:
        if temporario is not None:
            temporario.unlink(missing_ok=True)


def numero(valor, minimo, maximo):
    return (
        type(valor) in (int, float)
        and minimo <= valor <= maximo
        and math.isfinite(valor)
    )


class Planta:
    """Representa a física simplificada, sem escolher ações para atingir metas."""

    def __init__(self, seed=7, sessao=None, timeout_comando=5):
        if type(timeout_comando) is not int or timeout_comando < 1:
            raise ValueError("timeout_comando deve ser inteiro positivo")
        self.sessao = sessao or str(uuid4())
        self.rng = random.Random(seed)
        self.ciclo = 0
        self.nivel = 50.0
        self.temperatura = 25.0
        self.atuadores = {"bomba": 0.0, "aquecedor": 0.0}
        self.ultimo_comando = 0
        self.ciclo_comando = 0
        self.timeout_comando = timeout_comando
        self.erro_comando = None
        self.motivo_seguro = "sem_comando"

    def seguro(self, motivo):
        self.atuadores = {"bomba": 0.0, "aquecedor": 0.0}
        self.motivo_seguro = motivo

    def receber(self, comando):
        if not isinstance(comando, dict):
            raise ValueError("comando deve ser objeto JSON")
        if type(comando.get("versao")) is not int or comando["versao"] != 1:
            raise ValueError("versao deve ser 1")
        if comando.get("sessao") != self.sessao:
            raise ValueError("sessao diferente da planta atual")
        identificador = comando.get("id")
        if type(identificador) is not int or identificador <= 0:
            raise ValueError("id deve ser inteiro positivo")
        atuadores = comando.get("atuadores")
        if not isinstance(atuadores, dict) or set(atuadores) != {"bomba", "aquecedor"}:
            raise ValueError("atuadores deve conter somente bomba e aquecedor")
        if not all(numero(v, 0, 100) for v in atuadores.values()):
            raise ValueError("atuacoes devem ser numeros finitos entre 0 e 100")
        if identificador < self.ultimo_comando:
            raise ValueError("id antigo; envie um id maior")
        if identificador == self.ultimo_comando:
            return False  # Releitura do arquivo não renova a validade do comando.
        self.atuadores = dict(atuadores)
        self.ultimo_comando = identificador
        self.ciclo_comando = self.ciclo
        self.erro_comando = None
        self.motivo_seguro = None
        return True

    def passo(self, caminho_comando):
        self.ciclo += 1
        try:
            with Path(caminho_comando).open(encoding="utf-8") as arquivo:
                self.receber(json.load(arquivo))
        except FileNotFoundError:
            self.erro_comando = "arquivo de comando ausente"
            self.seguro("sem_arquivo")
        except (ValueError, OSError) as erro:
            self.erro_comando = str(erro)
            self.seguro("comando_invalido")
        if self.ciclo - self.ciclo_comando >= self.timeout_comando:
            self.seguro("comando_expirado")

        # Cada passo é um ciclo simulado; o intervalo de execução apenas regula a exibição.
        delta_nivel = 3.0 * self.atuadores["bomba"] / 100 - 1.0
        self.nivel = min(100.0, max(0.0, self.nivel + delta_nivel + self.rng.uniform(-0.2, 0.2)))
        delta_temp = 1.5 * self.atuadores["aquecedor"] / 100 - 0.04 * (self.temperatura - 20)
        self.temperatura = min(100.0, max(0.0, self.temperatura + delta_temp + self.rng.uniform(-0.1, 0.1)))
        instante = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        return {
            "versao": 1, "sessao": self.sessao, "ciclo": self.ciclo,
            "timestamp": instante,
            "leituras": [
                {"tag": "LT-01", "tipo": "nivel", "valor": round(self.nivel, 3),
                 "unidade": "%", "status": "operando"},
                {"tag": "TT-01", "tipo": "temperatura", "valor": round(self.temperatura, 3),
                 "unidade": "C", "status": "operando"},
            ],
            "atuadores": dict(self.atuadores), "ultimo_comando": self.ultimo_comando,
            "erro_comando": self.erro_comando, "motivo_seguro": self.motivo_seguro,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dados", type=Path, default=Path("dados"))
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--passos", type=int, default=0, help="0 executa ate Ctrl+C")
    parser.add_argument("--intervalo", type=float, default=1)
    parser.add_argument("--timeout-comando", type=int, default=5)
    args = parser.parse_args()
    if args.passos < 0 or not numero(args.intervalo, 0, 3600) or args.timeout_comando < 1:
        parser.error("passos >= 0, intervalo entre 0 e 3600 e timeout-comando >= 1")
    planta = Planta(seed=args.seed, timeout_comando=args.timeout_comando)
    print(f"Planta iniciada: sessao={planta.sessao}", flush=True)
    try:
        while args.passos == 0 or planta.ciclo < args.passos:
            estado = planta.passo(args.dados / "comando_planta.json")
            gravar_json(args.dados / "planta.json", estado)
            print(f"ciclo={estado['ciclo']} nivel={planta.nivel:.1f}% "
                  f"temperatura={planta.temperatura:.1f}C ack={planta.ultimo_comando}", flush=True)
            if args.passos == 0 or planta.ciclo < args.passos:
                time.sleep(args.intervalo)
    except KeyboardInterrupt:
        print("\nPlanta encerrada; reiniciar cria uma nova sessao.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
