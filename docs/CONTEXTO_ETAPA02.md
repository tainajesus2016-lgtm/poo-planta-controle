# Contexto — implementar a etapa 02 do controlador

> Documento de apoio para iniciar uma nova sessão de trabalho. Ele não substitui
> `docs/CONTRATO_CONTROLADOR.md` nem `docs/CONTRATO.md`.

## Objetivo desta sessão

Implementar a **etapa 02 (estratégia, estado e comando)** do controlador C++ do
projeto `poo-planta-controle`, fazendo `make test ETAPA=02 PYTHON=python3`
passar, sem implementar a etapa 03 (supervisor/Streamlit).

Nesta etapa entram:

- `Histerese::decidir()`;
- `Arquivos::gravar_comando()`;
- `Arquivos::carregar_estado()` e `Arquivos::salvar_estado()`;
- a reconciliação de ids e a publicação do comando em `Controlador::executar()`.

## Ambiente

- Projeto no WSL: `/mnt/c/Users/user/Desktop/trabalho/poo-planta-controle`
- Terminal: WSL (Ubuntu), com `bash`; execute os comandos na raiz do projeto.
- Python disponível como `python3` (o WSL não tem `python3`).
- Make disponível como `make`.
- Compilador: `g++` (C++17), flags do Makefile:
  `-Wall -Wextra -Werror -pedantic`
- Comando da etapa: `make test ETAPA=02 PYTHON=python3`
- O binário gerado é `build/controlador` (sem `.exe` no WSL).

## Estado atual do repositório

- Branch atual: `projeto/01-aquisicao`
- Commit base: `35b102b`
- `ETAPA_ATUAL` contém `01`.
- Há mudanças **não commitadas** da etapa 01. **Não descarte nem faça commit sem
  o usuário pedir.**
- Rode `git status --short` antes de começar.
- A etapa 01 já passa neste ambiente: 12 testes da planta, 3 de aquisição e 1
  executável autoral C++ (`autorais_aquisicao`), com `Resumo: PASSOU`.
- `controlador_estado.json` ainda não existe em `dados/`.
- `controlador/include/nlohmann/json.hpp` já está presente.
- **Não altere** `planta/`, `tests/test_planta.py`, `tests/test_controlador.py`
  nem `docs/CONTRATO.md`.

## O que já existe e deve ser preservado

### `controlador/modelo.hpp`

Já há as estruturas necessárias:

```cpp
struct Atuadores {
    double bomba = 0.0;
    double aquecedor = 0.0;
};

struct Comando {
    int versao = 1;
    std::string sessao;
    int id = 0;
    Atuadores atuadores;
};

struct EstadoControlador {
    int versao = 1;
    std::string sessao;
    int ultimo_ciclo_processado = 0;
    int ultimo_id_confirmado = 0;
    std::optional<int> id_pendente;
    int ciclo_emissao = 0;
    std::optional<Atuadores> atuadores_pendentes;
};
```

`Leitura`, `Aquisicao` e `Alarme` já são usados pela etapa 01.

### `controlador/contrato.hpp`

Constantes já definidas e que devem ser usadas:

```cpp
contrato::versao = 1
contrato::nivel_liga = 45.0
contrato::nivel_desliga = 55.0
contrato::temperatura_liga = 24.0
contrato::temperatura_desliga = 28.0
contrato::timeout_comando_ciclos = 5
```

### `controlador/arquivos.cpp`

A etapa 01 já deixou funções auxiliares reutilizáveis:

- `exigir_json(bool, std::string)`;
- `campo_json(const json&, const char*)`;
- `ler_inteiro_json(...)`;
- `ler_numero_json(...)`;
- `ler_texto_json(...)`;
- `ler_texto_ou_nulo(...)`;
- `ler_atuadores(...)`.

A gravação de `telemetria.json` já usa o padrão atômico:

1. grava em `<destino>.tmp` no mesmo diretório;
2. faz `flush`, fecha;
3. substitui o destino com `std::filesystem::rename`;
4. remove o temporário em caso de erro.

Reaproveite o mesmo padrão (ou extraia um auxiliar local) para comando e estado.

### `controlador/controlador.cpp`

A etapa 01 já tem:

- `encontrar_leitura(aquisicao, tag)`;
- `calcular_alarmes(aquisicao, obsolescencia)`;
- `idade_dos_dados(timestamp)`;
- o fluxo de aquisição, validação e telemetria;
- os códigos de saída `3` (aquisição inválida) e `4` (falha de gravação).

O modo contínuo já existe, mas ainda não decide nem publica comando.

## O que a etapa 02 exige (testes reais)

Arquivo `tests/test_controlador.py`, classe `TestAtuacao`, que herda de
`TestAquisicao`. Portanto, `make test ETAPA=02` executa os 3 testes de aquisição
**e mais** os 2 de atuação:

1. `test_comando_aplicado_no_proximo_ciclo`
   - após `controlador --dados DIR --uma-vez`, deve existir
     `comando_planta.json`;
   - `sessao` deve ser igual à da aquisição;
   - `id` deve ser `int` e maior que `ultimo_comando` da planta;
   - `atuadores` deve ter exatamente `bomba` e `aquecedor`, com números entre
     `0` e `100`;
   - a planta, ao dar o próximo passo, deve aceitar o comando:
     `erro_comando == null`, `ultimo_comando == id` e atuadores iguais.

2. `test_mesmo_ciclo_nao_gera_novo_comando`
   - executar duas vezes sobre a mesma aquisição;
   - o conteúdo de `comando_planta.json` deve permanecer **idêntico**;
   - não pode surgir um id novo.

Além disso, o runner continua exigindo pelo menos um teste C++ autoral. O ideal é
acrescentar um `tests/cpp/autorais_controle.cpp` cobrindo histerese, estado e
comando.

## Regras da etapa 02, em detalhe

### 1. `Histerese::decidir()` — `controlador/estrategia.cpp`

Assinatura já existente:

```cpp
std::optional<Atuadores> Histerese::decidir(const Aquisicao& aquisicao);
```

Comportamento:

1. Localizar as leituras de tag `LT-01` e `TT-01`.
2. Se faltar qualquer uma, devolver `std::nullopt` **sem alterar** `atuais_`.
3. Se qualquer uma estiver com `status == "falha"` ou sem `valor`, devolver
   `std::nullopt` **sem alterar** `atuais_`.
4. Nível:
   - `valor <= 45.0` → `bomba = 100.0`;
   - `valor >= 55.0` → `bomba = 0.0`;
   - `45.0 < valor < 55.0` → mantém `atuais_.bomba`.
5. Temperatura:
   - `valor <= 24.0` → `aquecedor = 100.0`;
   - `valor >= 28.0` → `aquecedor = 0.0`;
   - `24.0 < valor < 28.0` → mantém `atuais_.aquecedor`.
6. Guardar o resultado em `atuais_` e devolvê-lo.

Pontos de atenção:

- Os limites são inclusivos: exatamente `45` liga a bomba, exatamente `55`
  desliga; exatamente `24` liga o aquecedor, exatamente `28` desliga.
- Use `Leitura::valor.value()` somente depois de conferir `has_value()`.
- Não use a leitura de `atuadores` da planta como estado inicial da histerese:
  o contrato define o estado inicial como `bomba = 0` e `aquecedor = 0`
  (`atuais_{}` já faz isso).
- A histerese é um objeto com memória. Em `Controlador::executar()`, crie uma
  única instância **fora** do laço contínuo, para ela sobreviver entre ciclos.
  Ao reiniciar o processo, ela volta para `0/0`, como manda o contrato.

### 2. `Arquivos::gravar_comando()` — `controlador/arquivos.cpp`

Montar exatamente:

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "id": 4,
  "atuadores": {"bomba": 100.0, "aquecedor": 0.0}
}
```

Regras:

- `versao`, `sessao` e `id` vêm de `Comando`; não invente valores.
- `atuadores` deve ter exatamente `bomba` e `aquecedor`.
- Gravar de forma atômica em `comando_planta.json`, no mesmo diretório.
- Em caso de erro, remover o temporário e lançar `std::runtime_error`.
- Não gravar nada quando a decisão for inválida (isso é responsabilidade do
  controlador, mas o método deve continuar sendo só serialização).

### 3. `Arquivos::carregar_estado()` — `controlador/arquivos.cpp`

Ler `controlador_estado.json` quando existir.

Estado vazio esperado (primeira execução):

```cpp
EstadoControlador estado;
// versao = 1
// sessao = ""
// ultimo_ciclo_processado = 0
// ultimo_id_confirmado = 0
// id_pendente = nullopt
// ciclo_emissao = 0
// atuadores_pendentes = nullopt
```

Regras:

- Se o arquivo **não existir**, devolver o estado vazio.
- Se o arquivo estiver **corrompido** (JSON inválido, raiz que não é objeto,
  campos com tipo errado, `versao` diferente de `1`, id negativo etc.), devolver
  o estado vazio. Não lançar exceção e não interromper o controlador.
- Se o arquivo for válido, converter todos os campos:

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "ultimo_ciclo_processado": 12,
  "ultimo_id_confirmado": 3,
  "id_pendente": 4,
  "ciclo_emissao": 12,
  "atuadores_pendentes": {"bomba": 100.0, "aquecedor": 0.0}
}
```

- Quando não houver pendente, os campos devem ser lidos como:
  `id_pendente: null`, `atuadores_pendentes: null`, `ciclo_emissao: 0`.
- Considere inválido um estado incoerente, por exemplo `id_pendente` presente
  sem `atuadores_pendentes`, ou `id_pendente <= 0`, ou `ciclo_emissao < 0`.
  Nesse caso, trate como corrompido e devolva o estado vazio.
- Pode reutilizar `campo_json`, `ler_inteiro_json`, `ler_texto_json`,
  `ler_atuadores` e `exigir_json`. Envolva o parse em `try/catch` para JSON
  inválido.

### 4. `Arquivos::salvar_estado()` — `controlador/arquivos.cpp`

Montar o JSON do estado com todos os campos do modelo e gravar de forma atômica
em `controlador_estado.json`.

- Preserve `null` em `id_pendente` e `atuadores_pendentes` quando não houver
  pendente.
- Use `contrato::versao` (ou `estado.versao`).
- Mesmo padrão atômico: temporário no mesmo diretório, `flush`, fechar, rename,
  remover temporário em caso de erro.

### 5. `Controlador::executar()` — `controlador/controlador.cpp`

Esta é a parte de orquestração. O fluxo recomendado para **cada aquisição
válida** é:

```text
ler planta.json
  -> erro de leitura/validação: stderr + return 3
     (não grava estado, comando nem telemetria)

carregar controlador_estado.json
  -> ausente/corrompido já vem como estado vazio

calcular alarmes e verificar obsolescência
  -> telemetria continua sendo publicada mesmo com alarme/obsolescência

reconciliar a sessão e os ids

decidir a atuação com Histerese::decidir()

montar o comando, se permitido

salvar o estado (antes de publicar o comando)

gravar o comando, se houver

gravar a telemetria

return 0
```

#### 5.1. Reconciliação de sessão e ids

Use a aquisição atual e o estado carregado.

1. **Sessão mudou** (`estado.sessao != aquisicao.sessao`):
   - descarte o ciclo anterior;
   - zere `id_pendente`, `ciclo_emissao` e `atuadores_pendentes`;
   - reinicie `ultimo_ciclo_processado` em `0`;
   - use `aquisicao.ultimo_comando` como base de `ultimo_id_confirmado`;
   - a aquisição atual passa a ser nova.

2. **Mesma sessão**:
   - se existe `id_pendente` e `aquisicao.ultimo_comando >= id_pendente`, o
     pendente foi confirmado:
     - `ultimo_id_confirmado = max(ultimo_id_confirmado, id_pendente)`;
     - limpe `id_pendente`, `ciclo_emissao` e `atuadores_pendentes`;
   - senão, se existe `id_pendente` e
     `aquisicao.ciclo - estado.ciclo_emissao >= contrato::timeout_comando_ciclos`
     (5), o pendente expirou:
     - limpe o pendente;
   - em qualquer caso, se `aquisicao.ultimo_comando > ultimo_id_confirmado`,
     atualize `ultimo_id_confirmado` com o valor da planta. Isso cobre comandos
     manuais ou estado perdido.

3. **Aquisição nova**:
   - é nova quando `aquisicao.ciclo != estado.ultimo_ciclo_processado` depois do
     tratamento de sessão;
   - ciclo igual é **reprocessamento** da mesma aquisição;
   - por segurança, não crie id novo quando
     `aquisicao.ciclo < estado.ultimo_ciclo_processado`.

#### 5.2. Quando emitir comando

Chame `decidir()` em toda aquisição válida para manter a memória da histerese,
mas só publique comando quando:

- a decisão tem valor (`std::optional` preenchido);
- nenhuma leitura obrigatória está ausente ou em falha;
- a aquisição **não** está obsoleta.

A obsolescência é a mesma do contrato: idade do `timestamp` maior que
`--obsolescencia`; `--obsolescencia 0` desliga a verificação. O alarme
`DADOS_OBSOLETOS` já sinaliza isso na telemetria.

Ordem de decisão:

1. **Se existe `id_pendente` ainda não confirmado e não expirado**, reescreva o
   **mesmo comando** (mesmo `id` e mesmos `atuadores_pendentes`). Não crie id
   novo. Isso também vale ao reprocessar o mesmo ciclo, e é o que permite
   recuperar uma publicação que falhou depois de o estado ter sido salvo.
2. **Se não existe pendente** e a aquisição é nova, crie um comando novo:
   - `novo_id = max(aquisicao.ultimo_comando, estado.ultimo_id_confirmado) + 1`;
   - defina `id_pendente = novo_id`;
   - defina `ciclo_emissao = aquisicao.ciclo`;
   - defina `atuadores_pendentes = decisao.value()`;
   - use a decisão atual da histerese.
3. **Se não existe pendente** e a aquisição é reprocessada (mesmo ciclo), **não
   crie id novo**. Se o comando anterior já foi confirmado, simplesmente não
   publique comando novo.
4. Se a decisão for inválida ou os dados estiverem obsoletos, **não publique
   comando**, mesmo que exista pendente. A regra explícita da etapa é: leitura
   inválida, ausente, em falha ou obsoleta não gera comando.

O id sempre deve satisfazer `id > aquisicao.ultimo_comando`. A fórmula acima
garante isso.

#### 5.3. Ordem de gravação

Para cada aquisição válida:

1. atualize `estado.ultimo_ciclo_processado = aquisicao.ciclo`;
2. `salvar_estado(estado)` **antes** de publicar o comando;
3. se houver comando, `gravar_comando(comando)`;
4. `gravar_telemetria(aquisicao, alarmes)`;
5. retorne `0`.

Se `salvar_estado`, `gravar_comando` ou `gravar_telemetria` falhar, registre no
`stderr` e retorne `4`.

Por que salvar o estado antes: se o processo morrer entre o estado e o comando, a
próxima execução encontra o `id_pendente` e reemite o mesmo id. Nunca gere um id
diferente nesse caso.

#### 5.4. Modo contínuo

- Crie `Arquivos` e `Histerese` uma única vez, fora do `while`.
- Em cada iteração, repita o fluxo acima.
- Erro transitório de aquisição: escreva no `stderr`, não apague a telemetria
  anterior e tente de novo no próximo intervalo.
- Não é necessário implementar sinais/Ctrl+C além do que já existe.

## Compatibilidade com a planta

`planta/simulador.py`:

- aceita `id == ultimo_comando` sem erro, mas **não renova** a validade
  (`receber` devolve `False`);
- rejeita `id < ultimo_comando`;
- aplica o comando e atualiza `ultimo_comando` e `atuadores`;
- zera os atuadores após 5 ciclos sem id novo, marcando
  `motivo_seguro = "comando_expirado"`.

Por isso:

- reemitir o mesmo id é seguro e não cria um novo comando aplicado;
- um id novo em toda aquisição nova é o que mantém a atuação viva;
- quando a planta confirma `id_pendente`, o próximo ciclo novo deve gerar outro
  id (mesmo que os valores de atuadores sejam iguais);
- ao reprocessar o mesmo ciclo, o arquivo de comando deve ficar byte a byte
  igual ao anterior. O teste compara o JSON recarregado, não bytes, mas o
  resultado deve ser o mesmo objeto.

## Testes autorais recomendados

Crie `tests/cpp/autorais_controle.cpp` (com `main` retornando `0`/`1`) cobrindo,
no mínimo:

### Histerese

- nível `45.0` e temperatura `24.0` → bomba `100`, aquecedor `100`;
- nível `55.0` e temperatura `28.0` → bomba `0`, aquecedor `0`;
- valores no meio (`50.0`, `26.0`) mantêm a decisão anterior: comece desligado e
  confirme `0`; depois ligue e confirme que continua `100`;
- falta `LT-01` → `std::nullopt` e `atuais_` preservado;
- falta `TT-01` → `std::nullopt`;
- `status == "falha"` → `std::nullopt` e estado preservado.

### Estado

- arquivo ausente → estado vazio;
- conteúdo `{` e JSON válido com tipos errados → estado vazio, sem exceção;
- `salvar_estado` seguido de `carregar_estado` preserva todos os campos,
  inclusive `nullopt`;
- estado sem pendente grava `null` nos dois campos opcionais e
  `ciclo_emissao = 0`.

### Comando

- `gravar_comando` produz exatamente `versao`, `sessao`, `id` e `atuadores`;
- `atuadores` contém apenas `bomba` e `aquecedor`;
- reler o arquivo confere os valores gravados.

### Controlador

- aquisição válida sem estado → cria comando com
  `id > aquisicao.ultimo_comando`;
- segunda execução no mesmo ciclo → comando idêntico;
- planta confirma o id e o controlador processa um ciclo novo → novo id;
- pendente não confirmado em ciclo novo → mesmo id é reemitido;
- pendente expira em `ciclo_emissao + 5` → deixa de ser reemitido e permite novo
  id em aquisição nova;
- leitura ausente/falha → nenhum `comando_planta.json` novo;
- aquisição obsoleta (`--obsolescencia` pequena e timestamp antigo) → telemetria
  é gravada, mas nenhum comando é emitido;
- `planta.json` inválido → retorno `3` e estado/telemetria/comando anteriores
  intocados.

Para os testes C++, use uma pasta temporária única, como já faz
`tests/cpp/autorais_aquisicao.cpp`, e remova ao final.

## Ordem sugerida de implementação

1. `Histerese::decidir()`.
2. `Arquivos::carregar_estado()` (incluindo estado vazio e corrompido).
3. `Arquivos::salvar_estado()` e `Arquivos::gravar_comando()` com gravação atômica.
4. `Controlador::executar()`: reconciliação → decisão → estado → comando →
   telemetria.
5. Teste autoral novo.
6. Rodar `make test ETAPA=02 PYTHON=python3`.
7. Atualizar `ETAPA_ATUAL` de `01` para `02`.
8. Rodar `make test PYTHON=python3` sem `ETAPA`.
9. **Não commitar** sem pedido explícito.

## Armadilhas comuns

- Criar um id novo ao reprocessar o mesmo `(sessao, ciclo)`. O teste de
  idempotência falha.
- Criar um id novo quando existe `id_pendente` ainda válido. O contrato manda
  reemitir o mesmo id.
- Esquecer de reconciliar a confirmação antes de decidir o próximo id. A planta
  pode já ter aceitado o pendente.
- Salvar o comando antes do estado. Se a publicação falhar, o estado não sabe
  que o id foi emitido.
- Tratar estado corrompido como erro fatal. O contrato manda tratar como
  primeira execução.
- Emitir comando com leitura em falha, ausente ou obsoleta.
- Gravar estado/comando/telemetria depois de uma falha de aquisição.
- Usar `std::filesystem::rename` sem tratar erro ou deixar o `.tmp` para trás.
- Recriar `Histerese` a cada iteração do laço contínuo: a memória da histerese
  se perde.
- Esquecer `valor.has_value()` antes de acessar `value()`.
- Usar `id = ultimo_comando` (igual) como comando novo. A planta não renova a
  validade.

## Regras de estilo

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

- supervisor/Streamlit;
- testes Python autorais;
- leitura de `telemetria.json` pelo supervisor;
- comandos de operador pelo supervisor;
- TCP;
- commit/push.

## Critério de conclusão

- `make test ETAPA=02 PYTHON=python3` termina com `Resumo: PASSOU`;
- os 3 testes de aquisição continuam passando;
- os 2 testes de atuação passam;
- o teste autoral C++ novo passa junto com `autorais_aquisicao`;
- `ETAPA_ATUAL` contém `02`;
- `make test PYTHON=python3` também termina com `PASSOU`.
