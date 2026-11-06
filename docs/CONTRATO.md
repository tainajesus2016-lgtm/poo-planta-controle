# Contrato local da planta — versão 1

Três processos independentes trocam snapshots JSON UTF-8. Execute todos na raiz do fork.
Cada arquivo tem um único escritor. O simulador é fornecido; o controlador e o
supervisório começam incompletos. Os exemplos abaixo descrevem dados, não uma
implementação de controle.

```text
planta Python --planta.json--> controlador C++ --telemetria.json--> Streamlit
      ^                              |
      +------comando_planta.json-----+
```

> **Ambiente de execução:** no WSL (Ubuntu), execute os comandos a partir da
> raiz do projeto e use `python3` nos comandos Python. O `python3.15` é
> específico do ambiente Windows e não existe no WSL.

## 1. Quem escreve e quem lê

| Arquivo em `dados/` | Escritor | Leitor | Significado |
|---|---|---|---|
| `planta.json` | planta | controlador | última aquisição e confirmação da atuação |
| `comando_planta.json` | controlador | planta | próxima atuação solicitada |
| `telemetria.json` | controlador | supervisor | aquisição validada, estados e alarmes |

O supervisor não grava comandos diretamente na planta nem decide o controle.
Na versão mínima, a operação é automática, com parâmetros configurados no controlador.
Comandos de operador pelo supervisor são uma extensão: exigem arquivo próprio,
validação e decisão pelo controlador, preservando o único escritor de cada arquivo.

## 2. Aquisição produzida pela planta

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

- `versao`: inteiro 1, não booleano.
- `sessao`: texto não vazio, gerado novamente a cada início da planta.
- `ciclo`: inteiro positivo crescente dentro da sessão. É a identidade da aquisição.
- `timestamp`: instante de aquisição em UTC, formato `AAAA-MM-DDTHH:MM:SSZ`.
  Vários ciclos rápidos podem compartilhar o mesmo segundo; não deduplique por timestamp.
- `leituras`: lista de objetos com tag, tipo, valor, unidade e status.
  Nível usa `%` e temperatura usa `C`; na planta inicial, ambos estão em `[0,100]`.
  Esses limites são didáticos. Números devem ser finitos, sem NaN ou infinito.
- `status`: a planta inicial produz `operando`. Uma extensão pode produzir `falha`
  com `valor: null`; não reaproveite uma leitura antiga como atual.
- `atuadores`: percentuais efetivamente aplicados no ciclo, não decisões futuras.
- `ultimo_comando`: maior id aceito na sessão, inicialmente zero.
- `erro_comando`: diagnóstico textual ou `null`.
- `motivo_seguro`: `sem_arquivo`, `comando_invalido`, `comando_expirado` ou `null`.

Os clientes devem validar forma, tipos, unidades e significado antes de usar os dados.
Uma nova sessão exige abandonar o ciclo e os ids da sessão anterior.

## 3. Atuação recebida pela planta

```json
{
  "versao": 1,
  "sessao": "exemplo-sessao",
  "id": 1,
  "atuadores": {"bomba": 75.0, "aquecedor": 20.0}
}
```

A sessão deve coincidir com a aquisição atual. `id` é inteiro positivo maior que
`ultimo_comando`; os dois atuadores são obrigatórios e aceitam números em `[0,100]`.
Booleanos, strings numéricas e chaves de atuadores desconhecidas são rejeitados.

O comando é considerado antes de calcular a próxima leitura. A confirmação vem
em `ultimo_comando` e nos atuadores de `planta.json`; gravar o arquivo não prova
que a planta o aplicou. Id repetido não reaplica nem renova um comando. Id menor
é inválido. Após 5 ciclos sem um id novo aceito, ambos os atuadores vão a zero.
Esse prazo é configurável com `--timeout-comando` e conta ciclos, não segundos.

Comando malformado, sessão errada ou arquivo ausente também colocam a planta no
estado seguro didático de atuadores zerados. Um novo id válido permite retomar.
Em planta real, o estado seguro depende do equipamento e requer outra análise.

O controlador deve publicar no máximo um novo comando por aquisição e guardar
o último par sessão/ciclo processado, inclusive entre chamadas `--uma-vez`.
Pode usar `dados/controlador_estado.json`, de escrita exclusiva do controlador.
Ao reiniciar, reconcilie o id confirmado e o id pendente antes de emitir outro.

## 4. Telemetria a implementar em C++

Copie da aquisição validada os campos `versao`, `sessao`, `ciclo`, `timestamp`,
`leituras`, `atuadores`, `ultimo_comando`, `erro_comando` e `motivo_seguro`.
Acrescente `alarmes`, uma lista inicialmente vazia. Cada alarme ativo terá
`codigo`, `mensagem` e `severidade` textuais, por exemplo:

```json
{"codigo": "NIVEL_BAIXO", "mensagem": "Nivel abaixo da faixa definida", "severidade": "aviso"}
```

Não confunda atuação observada com atuação solicitada. Se publicar a solicitação
na telemetria, use outro campo e documente sua associação ao id de comando.
Entrada inválida deve gerar diagnóstico e retorno diferente de zero na CLI,
sem sobrescrever o snapshot válido anterior com dados inventados.

O supervisor precisa mostrar a idade e a origem da aquisição. Um snapshot antigo
continua legível após o encerramento de um processo; isso não é nova medição.
Documente o limite de obsolescência escolhido, diferente do timeout de atuação.

## 5. Escrita e concorrência

Cada escritor grava primeiro em arquivo temporário no mesmo diretório, fecha-o e
substitui o destino por renomeação. Não escreva sobre o snapshot enquanto outro
processo o lê. A planta fornece `gravar_json`, baseada em `os.replace`.
Isso oferece publicação de documento completo em sistema local compatível;
não promete transação entre vários arquivos nem durabilidade diante de queda.

Em C++, use uma biblioteca JSON para parsing e escape de texto; uma opção é
[JSON for Modern C++](https://json.nlohmann.me/). Documente a instalação e ajuste
o Makefile/CI se adicionar dependências. Não use expressões regulares para analisar JSON.

## 6. Dinâmica e reprodução

Por ciclo, a bomba aumenta o nível em até 3 pontos, o consumo retira 1 ponto e
um ruído em `[-0.2,0.2]` perturba a leitura. O aquecedor acrescenta até 1.5 grau,
há relaxamento de 4% em direção a 20 C e ruído em `[-0.1,0.1]`.
O valor resultante fica entre 0 e 100. Esta aproximação não representa um modelo
físico calibrado. A planta não possui setpoint nem escolhe a potência dos atuadores.

A mesma semente, sequência de comandos e quantidade de ciclos reproduzem os
valores; sessão e timestamp variam. `--intervalo` regula a espera entre ciclos,
sem mudar as equações. Os snapshots guardam somente o estado mais recente:
um leitor lento pode perder ciclos. No projeto, registre o histórico no consumidor,
deduplicando por sessão/ciclo, sem prometer aquisição sem perdas.

## 7. Extensões compatíveis

A equipe pode adaptar o cenário, a dinâmica e a configuração. Documente tags,
unidades, faixas, atuadores e comportamento seguro; atualize os fixtures e os
testes de contrato afetados com justificativa no PR. Preserve os testes da
planta original ou mantenha a configuração original disponível para executá-los.
Separar o envelope do domínio facilita manter compatibilidade entre versões.

TCP é opcional: documente enquadramento, desconexão e preservação de sessão/ciclo.
A execução por arquivo deve continuar funcionando.

## Referências

- [Python — JSON](https://docs.python.org/3/library/json.html)
- [Python — os.replace](https://docs.python.org/3/library/os.html#os.replace)
- [Streamlit — AppTest](https://docs.streamlit.io/develop/api-reference/app-testing/st.testing.v1.apptest)
