# Simulação do Algoritmo de Tomasulo  
### Disciplina: Arquitetura de Computadores 3 — Engenharia de Computação
### ICEI/PUC Minas - 1/2025
### Orientação do professor Matheus Alcântara de Souza
### Alunos: João Gabriel Mendonça Geraime Teodoro, Lucas Meira Duque, Luís Felipe Henrique Campelo e Nathália de Carvalho Oliveira Cunha Guimarães

Este projeto implementa um simulador do algoritmo de Tomasulo, desenvolvido em C++, como parte da disciplina **Arquitetura de Computadores 3**. O simulador executa instruções com suporte a execução fora de ordem (out-of-order execution), utilizando conceitos de superescalaridade, renomeação de registradores e estações de reserva.

## Compilação

Para compilar o simulador, utilize o seguinte comando:

```bash
g++ -std=c++17 tomasulo_sim.cpp -o tomasulo_sim -Wall
```

---

## ▶️ Execução

### 🔄 Execução ciclo a ciclo

Execute o simulador informando o caminho para o arquivo de entrada (`.txt`):

```bash
./tomasulo_sim [caminho/para/input.txt]
```
### Template para o arquivo de entrada
```txt

CONFIG_BEGIN
CYCLES <TipoDeOperacao> <NumeroDeCiclos>
CYCLES <TipoDeOperacao> <NumeroDeCiclos>
...
UNITS <TipoDeUnidade> <NumeroDeUnidades>
UNITS <TipoDeUnidade> <NumeroDeUnidades>
...
MEM_UNITS <TipoDeOperacaoMemoria> <NumeroDeUnidades>
MEM_UNITS <TipoDeOperacaoMemoria> <NumeroDeUnidades>
...
CONFIG_END

INSTRUCTIONS_BEGIN
<Opcode> <OperandoDestino> <OperandoFonte1> <OperandoFonte2_ou_OffsetComBase> # Comentário opcional
<Opcode> <OperandoDestino> <OperandoFonte1> <OperandoFonte2_ou_OffsetComBase>
...
INSTRUCTIONS_END
```
Durante a simulação:

- Pressione **Enter** para avançar um ciclo por vez;
- Digite **`r`** e pressione **Enter** para pular direto para o final da simulação.

---

### ⏩ Execução direta até o final

Para executar a simulação diretamente até o final (sem interação ciclo a ciclo), utilize:

```bash
./tomasulo_sim [caminho/para/input.txt] run
```
