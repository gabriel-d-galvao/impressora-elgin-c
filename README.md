# impressora-elgin-c
Projeto em C para comunicação com impressoras térmicas Elgin via DLL
# Controle de Impressora Térmica Elgin (i8 / i9) em C

Projeto acadêmico desenvolvido em linguagem **C** para comunicação e envio de comandos diretos a impressoras térmicas **Elgin (modelos i8 e i9)** no Windows, utilizando a biblioteca nativa `E1_Impressora01.dll` (64 bits).

## Funcionalidades

- Configuração e abertura de comunicação com impressora Elgin (USB, Serial ou Rede)
- Impressão de textos formatados
- Geração e impressão de **Códigos de Barras** e **QR Codes**
- Emissão de sinal sonoro (*beep* / barulho da impressora)
- Impressão de cupons fiscais SAT (venda e cancelamento) a partir de arquivos XML

## Arquivos do Projeto

- `main.c`: Código-fonte principal em C com o sistema de login e menu de comandos.
- `E1_Impressora01.dll`: Biblioteca oficial da Elgin (**arquitetura 64 bits / x64**).
- `XMLSAT.xml`: Arquivo XML de exemplo incluído para teste de impressão de cupom SAT.
- `CANC_SAT.xml`: Arquivo XML de exemplo incluído para teste de cancelamento SAT.

---

## Pré-requisitos e Compilação

> **Atenção:** A biblioteca `E1_Impressora01.dll` é de **64 bits (x64)**. Por isso, é **obrigatório** utilizar um compilador C de 64 bits no Windows.  
> **Não utilize o Dev-C++ 4.9.9.2**, pois ele compila em 32 bits e falhará ao carregar a DLL (**Erro 193**).

### Ambientes recomendados
- **MSYS2** (usando o terminal *"MSYS2 MinGW x64"*)
- **Code::Blocks** configurado com **MinGW-w64**
- **TDM-GCC 64**

### Passo a passo para compilar

1. Clone este repositório ou baixe os arquivos em uma mesma pasta:
   ```bash
   git clone https://github.com/gabriel-d-galvao/impressora-elgin-c.git
   cd impressora-elgin-c
