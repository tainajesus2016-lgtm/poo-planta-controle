# Contexto — implementar a etapa 03 (supervisor Streamlit)

> Documento de apoio para iniciar uma nova sessão de trabalho. Ele não substitui
> `docs/CONTRATO.md`, `docs/CONTRATO_CONTROLADOR.md` nem `README.md`.

## Objetivo desta sessão

Implementar a **etapa 03 (supervisor com Streamlit)** do projeto
`poo-planta-controle`, fazendo `make test ETAPA=03` passar.

O supervisor **lê apenas `telemetria.json`** produzido pelo controlador C++.
Ele nunca lê `planta.json` como atalho e nunca escreve comandos na planta.

## Ambiente

- Projeto no WSL: `/mnt/c/Users/user/Desktop/trabalho/poo-planta-controle`
- Terminal: WSL (Ubuntu), com `bash`; execute os comandos na raiz do projeto.
- Python disponível como `python3` (o WSL não tem `python3.15`).
- Make disponível como `make`.
- Compilador C++: `g++` (C++17)
- Streamlit e demais dependências são instalados no ambiente virtual abaixo.
- O arquivo `requirements.txt` pede `streamlit>=1.41,<2`.
- `requirements-dev.txt` inclui `streamlit`, `coverage` e `gcovr`.

### Preparar o ambiente uma vez

```bash
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r requirements-dev.txt
```

Se o WSL informar que `ensurepip` não está disponível, instale os pacotes e
repita os comandos:

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip
```

Se o `.venv` atual foi criado no Windows, remova e recrie no WSL:

```bash
rm -rf .venv
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
```

### Comando de teste

```bash
make test ETAPA=03 PYTHON=.venv/bin/python
```

Também deve continuar funcionando, depois de atualizar `ETAPA_ATUAL`:

```bash
make test PYTHON=.venv/bin/python
```

## Estado atual do repositório

- Branch atual: `projeto/01-aquisicao`
- Commit base: `35b102b`
- `ETAPA_ATUAL` contém `02`
- As etapas 01 e 02 estão implementadas, mas **não commitadas**.
- Há mudanças não commitadas em `controlador/`, `docs/`, `tests/cpp/` e
  `ETAPA_ATUAL`. **Não descarte nada e não faça commit sem pedido explícito.**
- Rode `git status --short` antes de começar.
- `controlador/include/nlohmann/json.hpp` já existe.
- Já existem `tests/cpp/autorais_aquisicao.cpp` e
  `tests/cpp/autorais_controle.cpp`.
- A etapa 02 deve estar passando com
  `make test ETAPA=02 PYTHON=python3`.

Se for criar a branch `projeto/03-supervisor`, primeiro integre/commite a etapa
02 conforme o fluxo do README. Não faça isso automaticamente sem o usuário pedir.

## Estrutura atual

```text
controlador/          controlador C++ (etapas 01 e 02 prontas)
supervisor/
└── app.py            esqueleto Streamlit (incompleto)
tests/
├── test_controlador.py   testes fornecidos do controlador
├── test_planta.py        testes fornecidos da planta
├── test_supervisor.py    testes fornecidos do supervisor (etapa 03)
├── cpp/                  testes autorais C++
└── autorais_*.py         testes autorais Python (ainda não existem)
```

`supervisor/app.py` hoje apenas mostra um título, o diretório e um aviso de
"Supervisório incompleto". Ele deve ser substituído pela tela real.

## O que a etapa 03 exige (testes reais)

Arquivo `tests/test_supervisor.py`:

### 1. `test_apresenta_leitura_do_controlador`

1. Cria um diretório temporário.
2. Gera uma telemetria válida com `Planta(...).passo(...)`.
3. Troca o valor da primeira leitura para `63.25` e define `alarmes = []`.
4. Grava `telemetria.json`.
5. Define `POO_DADOS` para o diretório temporário.
6. Executa `AppTest.from_file("supervisor/app.py").run(timeout=20)`.
7. Verifica:
   - `tela.exception` está vazio;
   - existe pelo menos um `st.dataframe`;
   - o texto de todas as tabelas contém `63.25`;
   - o texto contém `LT-01`.

Consequências:

- A tabela deve ser montada a partir do arquivo, sem valor fixo.
- O valor `63.25` precisa aparecer exatamente como veio da telemetria.
- A tag `LT-01` precisa aparecer na tabela.

### 2. `test_ausencia_e_json_invalido_sao_visiveis`

Para dois casos:

- `telemetria.json` **ausente**;
- `telemetria.json` com conteúdo inválido `{`;

Verifica:

- `tela.exception` está vazio;
- existe `st.warning` **ou** `st.error`.

Consequências:

- Nunca deixe a exceção subir.
- Use `st.warning(...)` ou `st.error(...)` para avisar o operador.

### 3. Teste autoral Python obrigatório

`tools/testar.py` exige, a partir da etapa 03, pelo menos um arquivo
`tests/autorais_*.py`.

Atenção: os testes autorais Python são descobertos e executados em **todas as
etapas**, inclusive 01 e 02. Por isso, o teste autoral **não pode importar
Streamlit**, senão quebra as etapas anteriores quando o Python global não tem
Streamlit instalado.

## Regras do supervisor (contrato e README)

- Consumir somente `telemetria.json`.
- Não ler `planta.json`.
- Não escrever `comando_planta.json`.
- Mostrar:
  - tabela de leituras;
  - origem dos dados (caminho e/ou sessão);
  - idade da aquisição;
  - alarmes;
  - aviso ou erro quando o arquivo estiver ausente ou o JSON for inválido.
- A tela deve continuar legível quando a telemetria estiver antiga.
- O diretório vem de `POO_DADOS`; padrão `dados`.
- O supervisor não decide controle nem altera a planta.

## Arquitetura recomendada (simples e testável)

Separe a leitura/apresentação da interface Streamlit:

```text
supervisor/
├── __init__.py       vazio ou com docstring curta
├── telemetria.py     funções puras, sem Streamlit
└── app.py            tela Streamlit
```

### `supervisor/telemetria.py`

Funções pequenas, sem importar Streamlit:

```python
def carregar_telemetria(pasta):
    # Lê <pasta>/telemetria.json.
    # Devolve (dados, None) se estiver tudo certo.
    # Devolve (None, "mensagem") se o arquivo não existir ou o JSON for inválido.
    ...

def montar_linhas(telemetria):
    # Devolve uma lista de dicionários:
    # {"Tag": ..., "Tipo": ..., "Valor": ..., "Unidade": ..., "Status": ...}
    ...

def calcular_idade(timestamp, agora=None):
    # Devolve a idade em segundos, ou None se o timestamp for inválido.
    ...

def montar_alarmes(telemetria):
    # Devolve uma lista de dicionários:
    # {"Código": ..., "Mensagem": ..., "Severidade": ...}
    ...
```

Regras:

- `carregar_telemetria` deve tratar:
  - arquivo ausente (`FileNotFoundError` ou `Path.exists()`);
  - JSON inválido (`json.JSONDecodeError`);
  - raiz que não é objeto (por exemplo `[]`, `null`, `"texto"`).
- `montar_linhas` deve ignorar entradas malformadas com segurança, sem quebrar.
- `calcular_idade` deve aceitar o formato UTC `AAAA-MM-DDTHH:MM:SSZ` e devolver
  `None` quando não conseguir interpretar.
- `montar_alarmes` deve devolver `[]` quando a lista estiver ausente.

### `supervisor/app.py`

Fluxo simples:

```text
main():
    ler POO_DADOS (padrão "dados")
    montar caminho <dados>/telemetria.json
    telemetria, erro = carregar_telemetria(dados)
    se erro:
        st.warning(erro) ou st.error(erro)
        return
    mostrar título e origem
    mostrar sessão, ciclo, timestamp e idade
    linhas = montar_linhas(telemetria)
    se linhas:
        st.dataframe(linhas, use_container_width=True)
    senão:
        st.warning("Telemetria sem leituras.")
    mostrar seção de alarmes
    se alarmes:
        st.dataframe(alarmes, use_container_width=True)
    senão:
        st.info("Nenhum alarme ativo.")
```

Cuidados:

- Mantenha o `if __name__ == "__main__": main()` no final.
- Garanta que o import de `supervisor.telemetria` funcione tanto com
  `streamlit run supervisor/app.py` quanto com `AppTest.from_file(...)`.
  Uma forma simples é inserir a raiz do projeto em `sys.path` antes do import.
- **Não use `st.cache_data`**: os testes criam arquivos temporários e precisam
  ler o conteúdo atual a cada execução.
- Não use APIs experimentais/removidas do Streamlit. Fique em `st.title`,
  `st.caption`, `st.dataframe`, `st.info`, `st.warning`, `st.error`.
- Não coloque valores fixos de exemplo na tabela.

Sugestão de import em `app.py`:

```python
import os
import sys
from pathlib import Path

import streamlit as st

# Permite importar "supervisor.telemetria" ao rodar como script.
RAIZ = Path(__file__).resolve().parents[1]
if str(RAIZ) not in sys.path:
    sys.path.insert(0, str(RAIZ))

from supervisor.telemetria import (
    calcular_idade,
    carregar_telemetria,
    montar_alarmes,
    montar_linhas,
)
```

## Teste autoral Python recomendado

Crie `tests/autorais_supervisor.py` usando `unittest` e apenas a biblioteca
padrão + `supervisor.telemetria`:

- arquivo ausente → devolve erro, sem exceção;
- JSON inválido (`{`) → devolve erro, sem exceção;
- raiz que não é objeto (`[]`) → devolve erro;
- telemetria válida → `montar_linhas` devolve Tag/Tipo/Valor/Unidade/Status;
- valor `63.25` e tag `LT-01` são preservados;
- `calcular_idade` com timestamp válido e `agora` fixo devolve o valor esperado;
- `calcular_idade` com timestamp inválido devolve `None`;
- `montar_alarmes` com lista vazia devolve `[]`;
- `montar_alarmes` com um alarme devolve Código/Mensagem/Severidade.

Use `tempfile.TemporaryDirectory()` e escreva arquivos com
`Path.write_text(..., encoding="utf-8")`.

**Não importe Streamlit nesse arquivo.**

## Ordem sugerida de implementação

1. Criar `supervisor/__init__.py`.
2. Criar `supervisor/telemetria.py` com as funções puras.
3. Criar `tests/autorais_supervisor.py` e rodar os testes das funções puras.
4. Reescrever `supervisor/app.py` usando as funções.
5. Rodar `make test ETAPA=03 PYTHON=.venv/bin/python`.
6. Corrigir até os 2 testes de `test_supervisor.py` passarem.
7. Atualizar `ETAPA_ATUAL` de `02` para `03`.
8. Rodar `make test PYTHON=.venv/bin/python` sem `ETAPA`.
9. **Não commitar** sem pedido explícito.

## Critério de conclusão

- os 12 testes da planta continuam passando;
- os testes de aquisição e atuação do controlador continuam passando;
- os 2 testes de `test_supervisor.py` passam;
- pelo menos 1 `tests/autorais_*.py` é executado;
- a linha final é `Resumo: PASSOU`;
- `ETAPA_ATUAL` contém `03`.

## Fora de escopo

- alterar a planta, o controlador C++ ou os testes fornecidos;
- comandos de operador pelo supervisor;
- escrita em `comando_planta.json`;
- TCP;
- histórico persistente;
- login, banco de dados ou múltiplas páginas;
- commit/push.

## Armadilhas comuns

- Ler `planta.json` em vez de `telemetria.json`.
- Deixar a exceção subir quando o arquivo não existe ou o JSON é inválido.
- Mostrar valores fixos em vez dos valores do arquivo.
- Usar `st.cache_data`, fazendo o teste ler uma versão antiga.
- Importar Streamlit no teste autoral e quebrar as etapas 01/02.
- Não criar nenhum `st.dataframe`, fazendo o teste falhar.
- Usar `st.table` no lugar de `st.dataframe` (o teste procura `tela.dataframe`).
- Esquecer de tratar `leituras` ausente ou com tipo errado.
- Não atualizar `ETAPA_ATUAL` depois de passar.
