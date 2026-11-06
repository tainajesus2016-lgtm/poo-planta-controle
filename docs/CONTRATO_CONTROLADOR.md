# Contrato local do controlador — versão 1

Este documento complementa [o contrato da planta](CONTRATO.md). O contrato da
planta continua sendo a autoridade sobre `planta.json`; aqui definimos como o
controlador lê, valida, decide e publica.

O controlador é o único escritor de `telemetria.json`, `comando_planta.json` e
`controlador_estado.json`. Ele nunca escreve `planta.json` e nunca lê
`comando_planta.json` como se fosse uma decisão sua.

> **Ambiente de execução:** os testes deste projeto são executados no terminal
> WSL (Ubuntu), a partir da raiz do projeto, com `make`, `g++` e `python3`.
> Use `make test ETAPA=NN PYTHON=python3`; o `python3.15` é específico do
> ambiente Windows e não existe no WSL.

```text
planta.json --> controlador --> telemetria.json --> supervisor
                    |
                    +--> comando_planta.json --> planta
                    |
                    +--> controlador_estado.json (uso interno)
```

## 1. Arquivos e responsabilidades

| Arquivo em `dados/` | Escritor | Leitor | Conteúdo |
|---|---|---|---|
| `planta.json` | planta | controlador | última aquisição e confirmação da atuação |
| `telemetria.json` | controlador | supervisor | aquisição validada, estados e alarmes |
| `comando_planta.json` | controlador | planta | próxima atuação solicitada |
| `controlador_estado.json` | controlador | controlador | último ciclo processado, ids e comando pendente |

Cada arquivo tem um único escritor. O supervisor não grava comandos diretamente
na planta nem decide o controle.

## 2. Linha de comando

```text
controlador --dados DIRETORIO [--uma-vez] [--intervalo SEGUNDOS]
            [--obsolescencia SEGUNDOS]
controlador --ajuda
```

| Opção | Significado | Padrão |
|---|---|---|
| `--dados DIRETORIO` | pasta que contém `planta.json` | `dados` |
| `--uma-vez` | processa uma aquisição e encerra | desligado |
| `--intervalo SEGUNDOS` | espera entre leituras no modo contínuo | `1` |
| `--obsolescencia SEGUNDOS` | idade máxima aceitável da aquisição | `10` |
| `--ajuda` | mostra o uso e encerra | — |

### Códigos de saída

| Código | Significado |
|---|---|
| `0` | execução bem-sucedida |
| `2` | argumento inválido ou uso incorreto |
| `3` | `planta.json` ausente, ilegível ou inválido |
| `4` | falha ao gravar um arquivo de saída |
| `5` | erro interno inesperado |

`--uma-vez` deve funcionar mesmo quando o mesmo ciclo já foi processado antes.
Nesse caso, a telemetria pode ser reescrita com o mesmo conteúdo, mas **nenhum
comando novo** pode ser emitido.

## 3. Entrada: `planta.json`

Exemplo mínimo aceito:

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "ciclo": 12,
  "timestamp": "2026-10-21T18:00:00Z",
  "leituras": [
    {"tag": "LT-01", "tipo": "nivel", "valor": 48.5, "unidade": "%", "status": "operando"},
    {"tag": "TT-01", "tipo": "temperatura", "valor": 26.2, "unidade": "C", "status": "operando"}
  ],
  "atuadores": {"bomba": 0.0, "aquecedor": 0.0},
  "ultimo_comando": 0,
  "erro_comando": null,
  "motivo_seguro": "sem_arquivo"
}
```

### 3.1. Regras de validação

- O documento deve ser um objeto JSON. `null`, lista, texto ou objeto vazio são inválidos.
- `versao`: inteiro exatamente `1`. Booleano não é inteiro válido.
- `sessao`: texto não vazio.
- `ciclo`: inteiro positivo e crescente dentro da sessão.
- `timestamp`: texto no formato UTC `AAAA-MM-DDTHH:MM:SSZ`.
- `leituras`: lista não vazia de objetos.
  - `tag`: texto não vazio e único na lista.
  - `tipo`: `nivel` ou `temperatura` na versão 1.
  - `unidade`: `%` para `nivel` e `C` para `temperatura`.
  - `valor`: número finito entre `0` e `100`, ou `null` quando `status` for `falha`.
  - `status`: `operando` ou `falha`.
  - Números não podem ser `NaN`, infinito, booleanos ou textos numéricos.
- `atuadores`: objeto com exatamente `bomba` e `aquecedor`, ambos números finitos
  entre `0` e `100`. Chaves extras, booleanos e textos são inválidos.
- `ultimo_comando`: inteiro não negativo.
- `erro_comando`: texto ou `null`.
- `motivo_seguro`: `sem_arquivo`, `comando_invalido`, `comando_expirado`,
  `sem_comando` ou `null`.
- Campos desconhecidos no envelope podem ser ignorados para compatibilidade.
  Dentro de `leituras`, apenas os campos documentados são usados.

### 3.2. Entrada inválida

Quando a entrada for inválida ou estiver ausente:

- o controlador escreve um diagnóstico no `stderr`;
- retorna código diferente de zero;
- **não sobrescreve** uma `telemetria.json` válida anterior;
- não grava `comando_planta.json` nem altera `controlador_estado.json`.

No modo contínuo, o erro é registrado e a leitura é tentada novamente no próximo
intervalo. No modo `--uma-vez`, o processo encerra com código `3`.

### 3.3. Sessão nova

`(sessao, ciclo)` identifica a aquisição. Quando `sessao` muda, o controlador
descarta o ciclo anterior, zera o comando pendente e reinicia a numeração de ids
a partir do `ultimo_comando` informado pela planta.

## 4. Saída: `telemetria.json`

A telemetria deve conter os campos validados da aquisição e uma lista de alarmes:

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "ciclo": 12,
  "timestamp": "2026-10-21T18:00:00Z",
  "leituras": [
    {"tag": "LT-01", "tipo": "nivel", "valor": 44.0, "unidade": "%", "status": "operando"},
    {"tag": "TT-01", "tipo": "temperatura", "valor": 23.5, "unidade": "C", "status": "operando"}
  ],
  "atuadores": {"bomba": 0.0, "aquecedor": 0.0},
  "ultimo_comando": 3,
  "erro_comando": null,
  "motivo_seguro": null,
  "alarmes": [
    {
      "codigo": "NIVEL_BAIXO",
      "mensagem": "Nivel abaixo da faixa operacional",
      "severidade": "aviso"
    }
  ]
}
```

Regras:

- Copiar `versao`, `sessao`, `ciclo`, `timestamp`, `leituras`, `atuadores`,
  `ultimo_comando`, `erro_comando` e `motivo_seguro` **sem inventar valores**.
- `atuadores` representa a atuação observada na planta, não a solicitação futura.
- `alarmes` deve existir sempre, mesmo que seja `[]`.
- Cada alarme tem `codigo`, `mensagem` e `severidade` textuais.
- Severidades permitidas: `info`, `aviso`, `critico`.
- A ordem dos alarmes deve ser determinística, seguindo a tabela abaixo.

### 4.1. Alarmes da versão 1

| Código | Condição | Severidade |
|---|---|---|
| `NIVEL_BAIXO` | `LT-01 < 45.0` | `aviso` |
| `NIVEL_ALTO` | `LT-01 > 55.0` | `aviso` |
| `TEMPERATURA_BAIXA` | `TT-01 < 24.0` | `aviso` |
| `TEMPERATURA_ALTA` | `TT-01 > 28.0` | `aviso` |
| `LEITURA_FALHA` | alguma leitura com `status: "falha"` | `critico` |
| `LEITURA_AUSENTE` | falta `LT-01` ou `TT-01` | `critico` |
| `COMANDO_REJEITADO` | `erro_comando` não é `null`, ou `motivo_seguro` é `comando_invalido` ou `comando_expirado` | `aviso` |
| `DADOS_OBSOLETOS` | idade do `timestamp` maior que `--obsolescencia` | `aviso` |

Alarmes são informativos: eles não substituem as leituras nem mudam a atuação
observada. Se a aquisição estiver obsoleta (`DADOS_OBSOLETOS`), a telemetria
ainda pode ser publicada, mas o controlador **não emite comando novo**.

`--obsolescencia 0` desliga a verificação de idade. O limite padrão é 10 segundos
e é diferente do timeout de atuação da planta, que é contado em ciclos.

## 5. Saída: `comando_planta.json`

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "id": 4,
  "atuadores": {"bomba": 100.0, "aquecedor": 0.0}
}
```

Regras:

- `versao` deve ser o inteiro `1`.
- `sessao` deve ser exatamente a sessão da aquisição validada.
- `id` deve ser inteiro positivo e maior que `ultimo_comando` da planta.
- `atuadores` deve conter exatamente `bomba` e `aquecedor`, com números finitos
  entre `0` e `100`.
- O controlador publica no máximo um comando por aquisição nova.
- Reprocessar o mesmo `(sessao, ciclo)` não pode criar um id novo.
- Para renovar a validade, o controlador publica um comando em toda aquisição
  nova, mesmo que os valores sejam iguais aos anteriores. O id, porém, deve ser
  novo.
- Entrada inválida, leitura ausente, leitura em falha ou dados obsoletos não
  geram comando. A planta aplica seu próprio estado seguro após 5 ciclos sem id
  novo.

### 5.1. Numeração e reconciliação

O estado persistido guarda o último id confirmado e o id pendente. A sequência é:

1. Se `planta.ultimo_comando >= id_pendente`, o comando pendente foi confirmado:
   atualize `ultimo_id_confirmado` e limpe o pendente.
2. Se existe id pendente não confirmado e ainda não expirou, reescreva o **mesmo**
   comando (mesmo id e mesmos atuadores) e não crie id novo.
3. Considere o pendente expirado quando
   `planta.ciclo - ciclo_emissao >= 5`. Nesse caso, limpe o pendente e permita
   um novo comando.
4. Sem pendente, o próximo id é
   `max(planta.ultimo_comando, ultimo_id_confirmado) + 1`.
5. Grave `controlador_estado.json` com o id pendente **antes** de publicar o
   comando. Se a publicação falhar, o estado permite repetir o mesmo id na
   próxima execução.
6. Se o estado estiver ausente ou corrompido, trate como primeira execução:
   use `ultimo_comando` da planta como base e não reaproveite ids antigos.

## 6. Política de controle

A política inicial é histerese simples, sem setpoint contínuo. A interface
`Estrategia` permite trocar a política depois sem alterar a aquisição.

| Grandeza | Liga em 100% | Desliga em 0% | Entre os limites |
|---|---|---|---|
| Nível (`LT-01`) | `<= 45.0` | `>= 55.0` | mantém a decisão anterior |
| Temperatura (`TT-01`) | `<= 24.0` | `>= 28.0` | mantém a decisão anterior |

- Estado inicial dos atuadores: `bomba = 0` e `aquecedor = 0`.
- A bomba e o aquecedor trabalham em liga/desliga: apenas `0` ou `100`.
- A histerese evita ligar e desligar a cada ciclo.
- Se faltar `LT-01`, faltar `TT-01` ou alguma leitura estiver em `falha`, a
  política não decide e o controlador não emite comando.
- A decisão é uma sugestão; a planta só confirma a atuação no ciclo seguinte.

## 7. Estado: `controlador_estado.json`

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

| Campo | Significado |
|---|---|
| `versao` | versão do formato do estado; iniciar em `1` |
| `sessao` | sessão a que o estado pertence |
| `ultimo_ciclo_processado` | último `ciclo` já processado |
| `ultimo_id_confirmado` | maior id que a planta confirmou |
| `id_pendente` | id publicado e ainda não confirmado, ou `null` |
| `ciclo_emissao` | ciclo em que o id pendente foi publicado |
| `atuadores_pendentes` | atuação do id pendente, ou `null` |

Quando não houver pendente, use `id_pendente: null` e
`atuadores_pendentes: null`, com `ciclo_emissao: 0`.

## 8. Escrita atômica e concorrência

- Grave primeiro em um arquivo temporário no mesmo diretório, feche-o e só então
  substitua o destino por renomeação.
- Não escreva diretamente sobre um arquivo que outro processo pode estar lendo.
- No Windows, a renomeação precisa substituir o destino existente
  (`MoveFileExW` com `MOVEFILE_REPLACE_EXISTING` ou equivalente da biblioteca).
  No Linux, `std::filesystem::rename`/`rename` substitui o destino.
- Remova arquivos temporários em caso de erro.
- Não há transação entre vários arquivos. A ordem recomendada é: estado, depois
  comando e telemetria. A planta continua sendo a autoridade sobre a atuação
  realmente aplicada.

## 9. Modo contínuo

- Ler `planta.json`, validar e comparar `(sessao, ciclo)` com o estado.
- Se for o mesmo ciclo, não emitir comando novo; dormir pelo `--intervalo`.
- Se for um ciclo novo, publicar telemetria e, quando a decisão for válida,
  publicar no máximo um comando.
- Se a sessão mudar, reiniciar o acompanhamento de ciclo e ids.
- Em erro transitório de leitura, registrar o diagnóstico e tentar de novo no
  próximo intervalo, sem apagar a última telemetria válida.

## 10. Biblioteca JSON e testes

- Use uma biblioteca JSON para parsing e escape de texto. A escolha do projeto é
  **nlohmann/json** (header-only).
- Coloque o cabeçalho em `controlador/include/nlohmann/json.hpp`. O Makefile já
  inclui `-Icontrolador/include`.
- Não use expressões regulares para analisar JSON.
- Testes C++ da equipe ficam em `tests/cpp/autorais_*.cpp`, com um `main` por
  arquivo. Retorne `0` quando passar e diferente de zero quando falhar.
- A partir da etapa 01, pelo menos um teste C++ autoral é obrigatório.
- Teste casos comuns, fronteiras e falhas. Para cada classe, verifique o
  resultado e o estado preservado após uma recusa.

## 11. Critérios de aceitação por etapa

### Etapa 01 — aquisição e telemetria

- `controlador --dados DIR --uma-vez` retorna `0` para uma aquisição válida.
- `telemetria.json` existe e preserva `versao`, `sessao`, `ciclo`, `timestamp`,
  `leituras`, `atuadores` e `ultimo_comando`.
- `alarmes` existe, mesmo que vazio.
- Entrada inválida retorna código diferente de zero e não sobrescreve a
  telemetria anterior.
- Arquivo ausente retorna código diferente de zero.
- Pelo menos um teste C++ autoral é executado.

### Etapa 02 — comando e confirmação

- O comando tem a mesma sessão da aquisição.
- O `id` é inteiro e maior que `ultimo_comando`.
- Os atuadores são exatamente `bomba` e `aquecedor`, entre `0` e `100`.
- A planta aceita o comando no ciclo seguinte.
- Reprocessar o mesmo ciclo não muda o comando.
- Reiniciar o controlador reconcilia id confirmado e id pendente.

### Etapa 03 — supervisor

- O supervisor lê `telemetria.json`, não `planta.json`.
- A tela mostra tabela de leituras, origem, idade, alarmes e avisos de arquivo
  ausente ou JSON inválido.
- Testes Python autorais cobrem a lógica de leitura e apresentação.

## 12. Estrutura C++ inicial

| Arquivo | Responsabilidade |
|---|---|
| `controlador/contrato.hpp` | constantes do contrato: versão, faixas, timeout e obsolescência |
| `controlador/modelo.hpp` | `Leitura`, `Aquisicao`, `Atuadores`, `Comando`, `Alarme`, `EstadoControlador` |
| `controlador/arquivos.hpp/.cpp` | `Arquivos`: toda a leitura e gravação dos quatro JSONs |
| `controlador/estrategia.hpp/.cpp` | `Estrategia` (interface) e `Histerese` (política inicial) |
| `controlador/controlador.hpp/.cpp` | orquestrar aquisição, decisão, estado e publicação |
| `controlador/main.cpp` | interpretar a CLI e chamar o controlador |
| `tests/cpp/autorais_*.cpp` | testes dos objetos da equipe |

A estrutura foi simplificada de propósito:

- **Uma classe de arquivos (`Arquivos`)** em vez de quatro classes separadas
  (`LeitorPlanta`, `PublicadorTelemetria`, `PublicadorComando`,
  `RepositorioEstado`). Assim, todos os caminhos e o formato dos JSONs ficam em
  um único lugar, mais fácil de acompanhar.
- **Nomes curtos** (`Estrategia`, `Histerese`, `Arquivos`) para reduzir a carga
  de vocabulário enquanto se aprende C++.
- O padrão **Strategy** continua preservado: `Controlador` depende de
  `Estrategia`, e `Histerese` é apenas a primeira implementação.
- `Arquivos` usa `std::string` para a pasta e para os caminhos; os ciclos e ids
  são `int`. Não é necessário `std::filesystem` para acompanhar o código.

Os arquivos `.cpp` criados nesta etapa são esqueletos: os métodos ainda lançam
`std::logic_error` com um `TODO`. Substitua um `TODO` por vez, começando pela
leitura de `planta.json`, e rode os testes após cada mudança.

### 12.1. Convenções para manter o código simples

O código deve ser legível para quem está começando. Palavras-chave e recursos
avançados só entram quando resolvem um problema real. Não use por hábito:

| Não use sem necessidade | Use no lugar | Motivo |
|---|---|---|
| `inline` | nada | Só é necessário em funções livres definidas em cabeçalhos. Neste projeto não há nenhuma. |
| `constexpr` | `const` | `const` já garante que o valor não muda; `constexpr` só é útil para cálculo em tempo de compilação. |
| `explicit` | construtor normal | O projeto não faz conversões implícitas de texto para objetos. |
| `final` | classe normal | Não precisamos impedir herança. |
| `std::move` | atribuição comum (`pasta_(pasta)`) | É micro-otimização; copiar um texto curto não é problema. |
| `namespace { ... }` | funções normais no `.cpp` | O arquivo `main.cpp` é único e não precisa dessa proteção. |
| `operator==` / `operator!=` | comparar os campos diretamente | Crie operadores apenas quando realmente forem usados. |

Regras que continuam valendo:

- `const` em valores fixos do contrato.
- `virtual` e `override` são mantidos porque o padrão **Strategy** depende
  deles; `override` também faz o compilador avisar se a assinatura estiver
  errada.
- `#pragma once` é o include guard padrão dos cabeçalhos.
- `std::optional` representa campos que aceitam `null` no JSON.
- `(void)parametro;` aparece apenas em esqueletos, para o compilador não reclamar
  de parâmetro ainda não usado. Remova essa linha ao implementar o método.
- Prefira nomes curtos e diretos (`Arquivos`, `Estrategia`, `Histerese`).

## 13. Extensões compatíveis

- Comandos de operador pelo supervisor exigem um arquivo próprio, validação e
  decisão final do controlador. O supervisor não deve escrever diretamente em
  `comando_planta.json`.
- TCP é opcional; a execução por arquivos deve continuar funcionando.
- Mudanças de tags, unidades, faixas, alarmes ou política exigem atualização
  deste contrato, dos fixtures e dos testes afetados.
- Preserve a possibilidade de executar a planta original com a configuração
  padrão para manter as regressões.
