# Como testar os objetos que você criou

Um objeto não está verificado apenas porque seu construtor foi chamado. Precisamos
mostrar o que ele faz com uma entrada conhecida, o resultado esperado e o estado
que preserva quando recusa uma operação. Os testes fornecidos acompanham o
transporte e a planta; você acrescenta os testes das suas decisões e objetos.

## 1. Começar com um teste C++ completo

Abra `exemplos/testes/medicao.hpp` e `exemplos/testes/autorais_medicao.cpp`.
O primeiro contém uma classe completa, com valor entre 0 e 100; o segundo contém
um programa completo de teste com `main`, criação do objeto, chamadas, verificação
de resultado e mensagens de falha. É um exemplo independente, sem controle automático.

Para conhecer o mecanismo, copie somente o teste para a pasta descoberta pelo Make:

```bash
cp exemplos/testes/autorais_medicao.cpp tests/cpp/autorais_medicao.cpp
make test ETAPA=00
```

Procure estas linhas, além dos testes da planta:

```text
AUTORAL C++: build/autorais/autorais_medicao
OK: Medicao, fronteiras e preservacao de estado
```

O exemplo verifica valor inicial, fronteiras 0 e 100, rejeição de -1 e preservação
do estado após a rejeição. Ele fica em `exemplos/` para não ser confundido com
uma classe autoral do controlador e não inflar a cobertura do código do projeto.
Copiar este exemplo, sozinho, não testa os objetos que você desenvolver.

Agora, no arquivo copiado, troque temporariamente a primeira comparação de
`medicao.valor() == 50` para `medicao.valor() == 51` e repita `make test ETAPA=00`.
A execução deve falhar com `construtor perdeu a medicao inicial` e o Make deve
retornar erro. Restaure 50 e confirme que passa. Esse pequeno experimento mostra
que o teste consegue detectar uma saída diferente da esperada.

Ao criar suas classes, deixe `main.cpp` coordenar a execução e separe a lógica
em `.hpp`/`.cpp` dentro de `controlador/`. Escreva arquivos próprios em
`tests/cpp/autorais_*.cpp`, incluindo os cabeçalhos dessas classes. O Makefile
compila as implementações e liga-as a cada teste, sem ligar o main do aplicativo.
Cada teste precisa de um `main` que retorne 0 ao passar e valor diferente de zero
ao falhar; o exemplo demonstra esse contrato. Nomes de arquivos/pastas sem espaços
mantêm o uso do Make simples neste starter.

## 2. Entender o mesmo ciclo em Python

Leia o programa completo `exemplos/testes/autorais_planta.py`. Ele instancia o
objeto fornecido `Planta`, executa um ciclo sem comando e verifica três resultados.
Use-o primeiro para observar a descoberta de testes:

```bash
cp exemplos/testes/autorais_planta.py tests/autorais_planta.py
make test ETAPA=00
```

Agora a saída também inclui `test_primeiro_ciclo_mantem_atuadores_zerados` e `ok`.
Troque temporariamente a expectativa de ciclo 1 para 2 e confirme a falha com o
mesmo comando. Restaure o esperado antes de continuar.

Nos seus arquivos `tests/autorais_*.py`, importe o objeto real da equipe, herde de
`unittest.TestCase` e escreva métodos iniciados por `test_`. Teste a lógica Python
de validação, repositório ou persistência separadamente da tela. Use banco e
arquivos temporários para que o teste não dependa de uma execução anterior.
O exemplo da planta ensina o mecanismo; ele não substitui testes dos seus objetos.

## 3. Escolher os casos dos seus objetos

Para cada classe, identifique responsabilidade e métodos públicos. Preencha uma
matriz no README; por exemplo, o objeto `Medicao` do exercício teria:

| Objeto/método | Comportamento | Entrada e resultado | Tipo de caso |
|---|---|---|---|
| `Medicao`/construtor | guardar valor válido | 50 → valor 50 | comum |
| `Medicao.atualizar` | aceitar extremos | 0 e 100 → valores preservados | fronteira |
| `Medicao.atualizar` | rejeitar fora da faixa sem perder o anterior | -1 após 100 → exceção e valor 100 | falha |

Crie a matriz equivalente para seus objetos: aquisição, políticas, comando,
alarmes e repositório, conforme sua arquitetura. Verifique especialmente a troca
de política, igualdade ao limite, recusa de entrada inválida e identidade do registro.
Nem toda classe exige todos os tipos de caso; justifique os que não se aplicam.

## 4. Ver todos os testes pelo Make

Execute `make test ETAPA=NN` na etapa correspondente. Depois de atualizar
`ETAPA_ATUAL`, `make test` usa essa etapa automaticamente. Desde a etapa 01,
há exigência de testes autorais C++; desde a etapa 03, também de testes Python.
No WSL, execute a partir da raiz do projeto e use `PYTHON=python3` quando não
estiver usando um ambiente virtual. Os novos casos são executados junto às
regressões anteriores. Se um teste
autoral falhar, o comando falha, mesmo que todos os contratos fornecidos passem.

Não basta testar manualmente no terminal, deixar um teste fora das pastas de
descoberta ou colocar verificações sem execução na entrega. Na demonstração,
localize seu teste na saída e explique qual comportamento ele verifica.

## 5. Medir e interpretar a cobertura

Prepare o ambiente uma vez, na raiz do projeto:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
make cobertura ETAPA=00 PYTHON=.venv/bin/python
```

Na etapa em desenvolvimento, substitua `00` pela etapa apropriada ou omita
`ETAPA` se `ETAPA_ATUAL` já estiver correto. O comando limpa somente suas saídas
de medição anteriores em `build/cobertura` e `build/relatorios`, recompila com
GCC/gcov e mede o Python com coverage.py, incluindo subprocessos Python.
Precisa de `g++` e `gcov` da mesma versão. A medição usa um build separado do normal.

Abra os relatórios HTML gerados em `build/relatorios/cpp/` e
`build/relatorios/python/`, começando pelo `index.html` de cada pasta. Eles marcam
linhas executadas e caminhos ainda não percorridos. C++ considera `controlador/`,
excluindo `vendor/` e `third_party/`; Python exclui testes, ferramentas, exemplos,
ambiente e build. Novos módulos Python do projeto entram na medição.
O relatório Python inclui a planta fornecida: analise os módulos autorais
separadamente, sem usar a cobertura da planta para afirmar qualidade da equipe.

Um número alto não garante resultado correto: um teste sem verificações pode
executar muitas linhas. O passo útil é localizar um caminho não executado,
decidir se ele importa e acrescentar um caso com resultado esperado. Registre
na matriz uma lacuna conhecida e a justificativa de cobertura da sua classe.
Não exclua código autoral do relatório para melhorar artificialmente o percentual.

A cobertura C++ pode mostrar ramificações geradas pelo compilador para exceções;
nem toda ramificação corresponde diretamente a um `if` escrito. Interprete a
métrica junto ao código e aos casos de teste, sem comparar diretamente os totais
de C++ e Python nem exigir 100% como prova de correção.
O relatório C++ considera o código que o compilador emitiu; um método de
cabeçalho nunca usado pode nem aparecer. Confira também a matriz de objetos,
em vez de concluir que um arquivo ausente foi testado.

## 6. Confirmar localmente e na CI

No PR, inclua o comando e resultado dos testes, os nomes dos novos casos, o link
da CI do commit e uma breve análise de cobertura dos objetos alterados.
A CI executa os testes e uma segunda execução instrumentada, publicando o
artefato `cobertura`; baixe a pasta completa para navegar entre os arquivos HTML.
Se os testes falharem, os relatórios disponíveis ainda ajudam o diagnóstico,
mas não transformam a execução em aprovada. Não há percentual mínimo automático:
a revisão considera relevância dos casos, resultado verificado e lacunas explicadas.

## Referências

- [Python — unittest](https://docs.python.org/3/library/unittest.html)
- [Coverage.py — cobertura de ramificações](https://coverage.readthedocs.io/en/latest/branch.html)
- [Coverage.py — subprocessos](https://coverage.readthedocs.io/en/latest/subprocess.html)
- [gcovr — relatórios HTML](https://gcovr.com/en/stable/output/html.html)
