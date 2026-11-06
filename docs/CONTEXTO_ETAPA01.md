# Contexto — implementar a etapa 01 do controlador

> Documento de apoio para iniciar uma nova sessão de trabalho. Ele não substitui
> `docs/CONTRATO_CONTROLADOR.md` nem `docs/CONTRATO.md`.

## Objetivo desta sessão

Implementar a **etapa 01 (aquisição e telemetria)** do controlador C++ do
projeto `poo-planta-controle`, fazendo `make test ETAPA=01 PYTHON=python3`
passar, sem implementar a etapa 02 (comando/estratégia/estado).

## Ambiente

- Projeto no WSL: `/mnt/c/Users/user/Desktop/trabalho/poo-planta-controle`
- Terminal: WSL (Ubuntu), com `bash`; execute os comandos na raiz do projeto.
- Python disponível como `python3` (o WSL não tem `python3.15`).
- Make disponível como `make`.
- Compilador: `g++` (C++17), flags do Makefile:
  `-Wall -Wextra -Werror -pedantic`
- Comando de teste: `make test ETAPA=01 PYTHON=python3`
- O binário gerado é `build/controlador` (sem `.exe` no WSL).
- A etapa 00 passa hoje com 12 testes da planta.

## Estado atual do repositório

- Branch atual: `projeto/01-aquisicao`
- Commit base: `35b102b`
- `ETAPA_ATUAL` contém `00`
- Há mudanças **não commitadas** da refatoração do controlador. **Não descarte
  nem faça commit sem o usuário pedir.**
- Rode `git status --short` antes de começar.
- O contrato do controlador está em `docs/CONTRATO_CONTROLADOR.md`
  (seções 1–13; a 12.1 tem convenções de estilo).
- O contrato da planta, autoritativo para `planta.json`, está em
  `docs/CONTRATO.md`.
- **Não altere** `planta/`, `tests/test_planta.py`, `tests/test_controlador.py`
  nem `docs/CONTRATO.md`.

## Estrutura atual do controlador

```text
controlador/
├── contrato.hpp        constantes: versao, nivel_liga=45, nivel_desliga=55,
│                       temperatura_liga=24, temperatura_desliga=28,
│                       limites 0..100, timeout 5 ciclos, intervalo 1s, obsolescencia 10s
├── modelo.hpp          structs: Atuadores, Leitura, Aquisicao, Alarme, Comando, EstadoControlador
├── arquivos.hpp/.cpp   classe Arquivos (TODA leitura/gravação)
├── estrategia.hpp/.cpp Estrategia (interface) + Histerese (etapa 02, ainda stub)
├── controlador.hpp/.cpp orquestração (ainda stub)
└── main.cpp            CLI pronta: --dados, --uma-vez, --intervalo, --obsolescencia, --ajuda
```

Assinaturas relevantes:

```cpp
class Arquivos {
public:
    Arquivos(std::string pasta);
    Aquisicao ler_planta() const;
    void gravar_telemetria(const Aquisicao& aquisicao,
                           const std::vector<Alarme>& alarmes) const;
    void gravar_comando(const Comando& comando) const;          // etapa 02
    EstadoControlador carregar_estado() const;                   // etapa 02
    void salvar_estado(const EstadoControlador& estado) const;   // etapa 02
private:
    std::string caminho(const std::string& nome) const; // pasta + "/" + nome
    std::string pasta_;
};

class Controlador {
public:
    Controlador(std::string pasta_dados);
    int executar(bool uma_vez, double intervalo, double obsolescencia);
private:
    std::string pasta_dados_;
};
```

Hoje os métodos de `Arquivos` e `Controlador::executar` lançam
`std::logic_error("TODO ...")`. `main.cpp` já está pronto.

## O que a etapa 01 exige (testes reais)

Arquivo `tests/test_controlador.py`, classe `TestAquisicao`:

1. `test_aquisicao_preserva_origem_e_grandezas`
   - Executar `controlador --dados DIR --uma-vez` deve retornar `0`.
   - Deve existir `telemetria.json`.
   - Os campos `versao`, `sessao`, `ciclo`, `timestamp`, `leituras`,
     `atuadores`, `ultimo_comando` devem ser **exatamente iguais** aos de
     `planta.json`.
   - `alarmes` deve ser uma lista, mesmo vazia.

2. `test_entrada_invalida_nao_vira_telemetria_nova`
   - Para cada conteúdo inválido de `planta.json`: `{`, `null`, `[]`, `{}`
   - Retorno deve ser diferente de `0`.
   - `telemetria.json` anterior (com `{"anterior": true}`) deve permanecer
     **intocado**.

3. `test_arquivo_ausente_sinaliza_falha`
   - Sem `planta.json`, retorno diferente de `0`.

Além disso, `tools/testar.py` exige **pelo menos um**
`tests/cpp/autorais_*.cpp` com `main` retornando `0`/diferente de `0`.

## Regras de validação de `planta.json` (do contrato)

- Raiz deve ser objeto JSON. `null`, lista, texto ou objeto vazio são inválidos.
- `versao`: inteiro exatamente `1` (booleano não vale).
- `sessao`: texto não vazio.
- `ciclo`: inteiro positivo e crescente dentro da sessão.
- `timestamp`: texto UTC no formato `AAAA-MM-DDTHH:MM:SSZ`.
- `leituras`: lista não vazia de objetos:
  - `tag`: texto não vazio e único.
  - `tipo`: `nivel` ou `temperatura`.
  - `unidade`: `%` para nível, `C` para temperatura.
  - `valor`: número finito em `[0,100]`, ou `null` quando `status` for `falha`.
  - `status`: `operando` ou `falha`.
- `atuadores`: objeto com exatamente `bomba` e `aquecedor`, números finitos em
  `[0,100]`.
- `ultimo_comando`: inteiro não negativo.
- `erro_comando`: texto ou `null`.
- `motivo_seguro`: `sem_arquivo`, `comando_invalido`, `comando_expirado`,
  `sem_comando` ou `null`.
- Campos desconhecidos no envelope podem ser ignorados.
- Números `NaN`, infinito, booleanos ou textos numéricos são inválidos.

## O que implementar, em ordem

### 1. Adicionar `nlohmann/json`

O cabeçalho deve ficar em:

```text
controlador/include/nlohmann/json.hpp
```

Sugestão de download (single header) no WSL:

```bash
mkdir -p controlador/include/nlohmann
curl -L -o controlador/include/nlohmann/json.hpp \
  https://github.com/nlohmann/json/releases/latest/download/json.hpp
```

Se a rede não funcionar, instale o pacote `nlohmann-json3-dev` com
`sudo apt install nlohmann-json3-dev` e copie `/usr/include/nlohmann/json.hpp`
para `controlador/include/nlohmann/json.hpp`. **Não use regex para analisar JSON.**

### 2. Implementar `Arquivos::ler_planta()`

- Abrir `<pasta>/planta.json` com `std::ifstream`.
- Se não abrir, lançar erro (por exemplo `std::runtime_error`).
- Parsear com `nlohmann::json`.
- Validar tudo da lista acima.
- Converter para `Aquisicao` e devolver.
- Não reaproveitar leitura antiga; não inventar valores.

### 3. Implementar `Arquivos::gravar_telemetria()`

- Montar JSON copiando **sem alterar**:
  `versao`, `sessao`, `ciclo`, `timestamp`, `leituras`, `atuadores`,
  `ultimo_comando`, `erro_comando`, `motivo_seguro`.
- Preserve `valor: null` quando existir.
- Acrescentar `"alarmes"` (lista).
- Escrita atômica: gravar em arquivo temporário no mesmo diretório, fechar e
  substituir o destino por renomeação.
  - Use `std::filesystem::rename` (no WSL/Linux ele substitui o destino).
  - Se falhar, remova o temporário e propague o erro.
- Não grave nada se a leitura/validação falhou.

### 4. Implementar `Controlador::executar()` para `--uma-vez`

Fluxo mínimo:

```text
ler planta
  -> se inválida/ausente: stderr + return 3 (sem tocar em telemetria)
  -> calcular alarmes
  -> gravar telemetria
  -> return 0
  -> se falha de escrita: return 4
```

No modo contínuo (`sem --uma-vez`), repetir o fluxo com pausa de `intervalo`
segundos; erro transitório não deve apagar a telemetria anterior. Ctrl+C
encerra.

**Não implemente comando, estado nem `Histerese` nesta etapa.**
`comando_planta.json` não deve ser criado.

### 5. Alarmes

Mínimo para o teste: `"alarmes": []`.

Ideal, conforme o contrato (recomendado):

| Código | Condição | Severidade |
|---|---|---|
| `NIVEL_BAIXO` | `LT-01 < 45.0` | `aviso` |
| `NIVEL_ALTO` | `LT-01 > 55.0` | `aviso` |
| `TEMPERATURA_BAIXA` | `TT-01 < 24.0` | `aviso` |
| `TEMPERATURA_ALTA` | `TT-01 > 28.0` | `aviso` |
| `LEITURA_FALHA` | alguma leitura com `status: "falha"` | `critico` |
| `LEITURA_AUSENTE` | falta `LT-01` ou `TT-01` | `critico` |
| `COMANDO_REJEITADO` | `erro_comando != null` ou `motivo_seguro` em `comando_invalido`/`comando_expirado` | `aviso` |
| `DADOS_OBSOLETOS` | idade do timestamp > `--obsolescencia` (0 desliga) | `aviso` |

Ordem determinística, seguindo a tabela.

### 6. Criar teste C++ autoral

Criar `tests/cpp/autorais_aquisicao.cpp` (ou nome parecido) com:

- `#include "arquivos.hpp"` (o Makefile passa `-Icontrolador`).
- Um `main` que retorna `0` ao passar e `1` ao falhar.
- Use o padrão do exemplo `exemplos/testes/autorais_medicao.cpp`:

```cpp
void verificar(bool condicao, const char* mensagem) {
    if (!condicao) throw std::runtime_error(mensagem);
}
```

Casos sugeridos:

- JSON válido → `ler_planta()` devolve sessão, ciclo, leituras e atuadores
  corretos.
- JSON inválido (`{}`, `null`, `{"versao": "1"}`) → lança exceção.
- Arquivo ausente → lança exceção.
- `gravar_telemetria` → reler o arquivo e conferir os campos preservados e
  `alarmes`.
- Uma falha não deve alterar a telemetria anterior.

Use `std::filesystem::temp_directory_path()` para criar uma pasta temporária
única. Remova ao final.

## Comando de verificação

```bash
make test ETAPA=01 PYTHON=python3
```

Critério de conclusão:

- 12 testes da planta passando;
- 3 testes `TestAquisicao` passando;
- pelo menos 1 `AUTORAL C++: build/autorais/...` com `OK`;
- linha final `Resumo: PASSOU`.

## Regras de estilo (importantes)

O usuário está aprendendo e pediu código simples. **Não use sem necessidade:**

- `inline`
- `constexpr` (use `const`)
- `explicit`
- `final`
- `std::move`
- `namespace { ... }`
- `operator==`/`operator!=` não usados

Mantenha `virtual`/`override` porque o padrão **Strategy** faz parte do trabalho.
`(void)param;` só em stubs. Detalhes na seção **12.1** de
`docs/CONTRATO_CONTROLADOR.md`.

## Fora de escopo (não fazer agora)

- `Histerese::decidir()`
- `gravar_comando()`
- `carregar_estado()` / `salvar_estado()`
- reconciliação de ids
- `comando_planta.json`
- supervisor/Streamlit
- commit/push

## Ao terminar

1. Atualizar `ETAPA_ATUAL` de `00` para `01` (o README pede no mesmo
   incremento).
2. Rodar novamente `make test PYTHON=python3` (sem `ETAPA`) e confirmar
   `PASSOU`.
3. Mostrar ao usuário: arquivos alterados, saída dos testes e o que ficou para
   a etapa 02.
4. **Não commitar** sem pedido explícito.
