#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/*
 * Controle da impressora Elgin (i8 / i9) em C, usando a DLL E1_Impressora01.dll.
 *
 * Execute na pasta que contem a DLL e os dois arquivos XML de exemplo.
 * A DLL fornecida e x64. Compile no Windows com GCC de 64 bits:
 *   gcc -std=c89 -Wall -Wextra -o main.exe main.c
 */

/* ===================== Ligacao com a DLL (equivalente a interface ImpressoraDLL do Java) ===================== */

/* Cada typedef descreve a "forma" de um comando da DLL: o que devolve e quais parametros recebe */
typedef int (*FnAbreConexaoImpressora)(int tipo, const char *modelo, const char *conexao, int param);
typedef int (*FnFechaConexaoImpressora)(void);
typedef int (*FnImpressaoTexto)(const char *dados, int posicao, int estilo, int tamanho);
typedef int (*FnCorte)(int avanco);
typedef int (*FnImpressaoQRCode)(const char *dados, int tamanho, int nivelCorrecao);
typedef int (*FnImpressaoCodigoBarras)(int tipo, const char *dados, int altura, int largura, int HRI);
typedef int (*FnAbreGavetaElgin)(void);
typedef int (*FnAbreGaveta)(int pino, int ti, int tf);
typedef int (*FnSinalSonoro)(int qtd, int tempoInicio, int tempoFim);
typedef int (*FnImprimeXMLSAT)(const char *dados, int param);
typedef int (*FnImprimeXMLCancelamentoSAT)(const char *dados, const char *assQRCode, int param);

/* Um ponteiro para cada comando. Depois de carregarDLL(), e so chamar: AbreConexaoImpressora(...) */
static FnAbreConexaoImpressora AbreConexaoImpressora;
static FnFechaConexaoImpressora FechaConexaoImpressora;
static FnImpressaoTexto ImpressaoTexto;
static FnCorte Corte;
static FnImpressaoQRCode ImpressaoQRCode;
static FnImpressaoCodigoBarras ImpressaoCodigoBarras;
static FnAbreGavetaElgin AbreGavetaElgin;
static FnAbreGaveta AbreGaveta;
static FnSinalSonoro SinalSonoro;
static FnImprimeXMLSAT ImprimeXMLSAT;
static FnImprimeXMLCancelamentoSAT ImprimeXMLCancelamentoSAT;

static HMODULE dll = NULL;

/* Converte cada endereco para o tipo exato da funcao exportada. */
#define CARREGAR_FUNCAO(nome) do { \
    nome = (Fn##nome)(void (*)(void))GetProcAddress(dll, #nome); \
    if (nome == NULL) { \
        fprintf(stderr, "Comando '%s' nao encontrado na DLL.\n", #nome); \
        ok = 0; \
    } \
} while (0)

static int carregarDLL(void) {
    int ok = 1;

    dll = LoadLibraryW(L".\\E1_Impressora01.dll");
    if (dll == NULL) {
        fprintf(stderr, "Falha ao carregar E1_Impressora01.dll (erro %lu).\n",
                (unsigned long)GetLastError());
        fprintf(stderr, "Execute na pasta da DLL e confira a arquitetura x64.\n");
        return 0;
    }

    CARREGAR_FUNCAO(AbreConexaoImpressora);
    CARREGAR_FUNCAO(FechaConexaoImpressora);
    CARREGAR_FUNCAO(ImpressaoTexto);
    CARREGAR_FUNCAO(Corte);
    CARREGAR_FUNCAO(ImpressaoQRCode);
    CARREGAR_FUNCAO(ImpressaoCodigoBarras);
    CARREGAR_FUNCAO(AbreGavetaElgin);
    CARREGAR_FUNCAO(AbreGaveta);
    CARREGAR_FUNCAO(SinalSonoro);
    CARREGAR_FUNCAO(ImprimeXMLSAT);
    CARREGAR_FUNCAO(ImprimeXMLCancelamentoSAT);
    if (!ok) {
        FreeLibrary(dll);
        dll = NULL;
    }
    return ok;
}

#undef CARREGAR_FUNCAO

/* ===================== Variaveis do programa ===================== */

static int conexaoAberta = 0;       /* 0 = fechada, 1 = aberta */
static int tipo = 1;                /* 1 = USB, 2 = Serial, 3 = Rede... */
static char modelo[50] = "i9";      /* "i8" ou "i9" */
static char conexao[50] = "USB";
static int parametro = 0;

/* Assinatura do QR Code de venda associada ao CANC_SAT.xml de exemplo. */
static const char *ASS_QRCODE =
    "Q5DLkpdRijIRGY6YSSNsTWK1TztHL1vD0V1Jc4spo/CEUqICEb9SFy82ym8EhBRZjbh3btsZhF+sjHqEMR159i4agru9x6KsepK/"
    "q0E2e5xlU5cv3m1woYfgHyOkWDNcSdMsS6bBh2Bpq6s89yJ9Q6qh/J8YHi306ce9Tqb/drKvN2XdE5noRSS32TAWuaQEVd7u+"
    "TrvXlOQsE3fHR1D5f1saUwQLPSdIv01NF6Ny7jZwjCwv1uNDgGZONJdlTJ6p0ccqnZvuE70aHOI09elpjEO6Cd+orI7XHHrFCwhFhAcbalc+"
    "ZfO5b/+vkyAHS6CYVFCDtYR9Hi5qgdk31v23w==";

/* ===================== Funcoes auxiliares ===================== */

/* Descarta linhas longas inteiras para que nao contaminem a proxima resposta. */
static int capturarEntrada(const char *mensagem, char *destino, int tamanho) {
    for (;;) {
        char *quebra;

        printf("%s", mensagem);
        fflush(stdout);
        if (fgets(destino, tamanho, stdin) == NULL) {
            return 0;
        }

        quebra = strchr(destino, '\n');
        if (quebra == NULL) {
            int caractere = getchar();
            if (caractere != '\n' && caractere != EOF) {
                do {
                    caractere = getchar();
                } while (caractere != '\n' && caractere != EOF);
                printf("Entrada muito longa. Maximo de %d bytes.\n", tamanho - 1);
                continue;
            }
            quebra = destino + strlen(destino);
        }

        if (quebra > destino && quebra[-1] == '\r') {
            --quebra;
        }
        *quebra = '\0';
        return 1;
    }
}

static int lerTexto(const char *mensagem, char *destino, int tamanho) {
    while (capturarEntrada(mensagem, destino, tamanho)) {
        if (destino[strspn(destino, " \t\r\v\f")] != '\0') {
            return 1;
        }
        printf("Digite um valor nao vazio.\n");
    }
    return 0;
}

static int lerInteiro(const char *mensagem, int minimo, int maximo, int *destino) {
    char linha[50];
    while (capturarEntrada(mensagem, linha, sizeof(linha))) {
        char *fim;
        long valor;

        errno = 0;
        valor = strtol(linha, &fim, 10);
        if (fim != linha && errno != ERANGE && valor >= minimo && valor <= maximo
            && fim[strspn(fim, " \t\r\v\f")] == '\0') {
            *destino = (int)valor;
            return 1;
        }
        printf("Digite um inteiro de %d a %d.\n", minimo, maximo);
    }
    return 0;
}

/* Quem chama deve liberar o conteudo com free(). */
static char *lerArquivo(const char *caminho) {
    long tamanho;
    size_t bytes, lidos;
    char *conteudo;
    int falhou;
    FILE *arq = fopen(caminho, "rb");
    if (arq == NULL) {
        return NULL;
    }

    tamanho = fseek(arq, 0, SEEK_END) == 0 ? ftell(arq) : -1;
    if (tamanho <= 0 || fseek(arq, 0, SEEK_SET) != 0) {
        fclose(arq);
        return NULL;
    }

    bytes = (size_t)tamanho;
    conteudo = malloc(bytes + 1);
    if (conteudo == NULL) {
        fclose(arq);
        return NULL;
    }

    lidos = fread(conteudo, 1, bytes, arq);
    falhou = ferror(arq);
    fclose(arq);
    if (lidos != bytes || falhou || memchr(conteudo, '\0', bytes) != NULL) {
        free(conteudo);
        return NULL;
    }

    conteudo[bytes] = '\0';
    return conteudo;
}

static int exigirConexao(void) {
    if (!conexaoAberta) {
        printf("Conexao fechada. Abra a conexao antes (opcao 2).\n");
    }
    return conexaoAberta;
}

static int cortarPapel(void) {
    int retorno = Corte(5);
    if (retorno != 0) {
        printf("Falha ao cortar o papel. Codigo: %d. Confira a impressora.\n", retorno);
    }
    return retorno == 0;
}

void configurarConexao(void) {
    int novoTipo, novoParametro = 0;
    char novoModelo[sizeof(modelo)], novaConexao[sizeof(conexao)] = "USB";

    if (conexaoAberta) {
        printf("Feche a conexao (opcao 3) antes de reconfigurar.\n");
        return;
    }

    if (!lerInteiro("Tipo (1 USB, 2 Serial, 3 Rede, 4 Bluetooth): ", 1, 4, &novoTipo)
        || !lerTexto("Modelo da impressora (ex.: i9): ", novoModelo, sizeof(novoModelo))) {
        return;
    }

    if (novoTipo != 1
        && !lerTexto("Endereco da conexao (COM, IP ou MAC): ", novaConexao, sizeof(novaConexao))) {
        return;
    }
    if (novoTipo == 2 && !lerInteiro("Baud rate (ex.: 115200): ", 1, INT_MAX, &novoParametro)) {
        return;
    }
    if (novoTipo == 3 && !lerInteiro("Porta TCP (ex.: 9100): ", 1, 65535, &novoParametro)) {
        return;
    }

    tipo = novoTipo;
    strcpy(modelo, novoModelo);
    strcpy(conexao, novaConexao);
    parametro = novoParametro;
    printf("Parametros de conexao configurados. Use a opcao 2 para abrir.\n");
}

void abrirConexao(void) {
    if (!conexaoAberta) {
        int retorno = AbreConexaoImpressora(tipo, modelo, conexao, parametro);
        if (retorno == 0) {
            conexaoAberta = 1;
            printf("Conexao aberta com sucesso.\n");
        } else {
            printf("Erro ao abrir conexao. Codigo de erro: %d\n", retorno);
        }
    } else {
        printf("Conexao ja esta aberta.\n");
    }
}

void fecharConexao(void) {
    if (conexaoAberta) {
        int retorno = FechaConexaoImpressora();
        if (retorno == 0) {
            conexaoAberta = 0;
            printf("Conexao fechada com sucesso.\n");
        } else {
            printf("Erro ao fechar conexao. Codigo: %d\n", retorno);
        }
    } else {
        printf("A conexao ja esta fechada.\n");
    }
}

enum { MAX_COPIAS = 100 };

static void imprimirCopias(const char *texto, int copias, int teste) {
    char conteudo[256];
    int numero;
    int concluidas = 0;

    for (numero = 1; numero <= copias; ++numero) {
        int retorno;

        if (teste) {
            /* Ambos os numeros estao entre 1 e MAX_COPIAS. */
            sprintf(conteudo, "TESTE DE IMPRESSAO\nCopia %d de %d\n", numero, copias);
        } else {
            sprintf(conteudo, "%.*s\n", (int)sizeof(conteudo) - 2, texto);
        }

        retorno = ImpressaoTexto(conteudo, 1, teste ? 0 : 4, 0);
        if (retorno != 0) {
            printf("Falha ao enviar a copia %d de %d. Codigo: %d.\n", numero, copias, retorno);
            break;
        }
        if (!cortarPapel()) {
            printf("Copia %d de %d enviada, mas o corte falhou.\n", numero, copias);
            break;
        }

        ++concluidas;
        printf("Copia %d de %d enviada e cortada.\n", numero, copias);
    }

    printf("Copias concluidas: %d de %d.\n", concluidas, copias);
}

void imprimirTexto(void) {
    char texto[200];
    int copias;

    if (!exigirConexao()) {
        return;
    }

    if (lerTexto("Texto a imprimir: ", texto, sizeof(texto))
        && lerInteiro("Quantas copias (1 a 100)? ", 1, MAX_COPIAS, &copias)) {
        imprimirCopias(texto, copias, 0);
    }
}

void imprimirCopiasTeste(void) {
    int copias;

    if (!exigirConexao()) {
        return;
    }

    if (lerInteiro("Quantas copias de teste (1 a 100)? ", 1, MAX_COPIAS, &copias)) {
        imprimirCopias(NULL, copias, 1);
    }
}

void imprimirQRCode(void) {
    char dados[200];
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    if (!lerTexto("Conteudo do QR Code: ", dados, sizeof(dados))) {
        return;
    }

    retorno = ImpressaoQRCode(dados, 4, 2);
    if (retorno == 0) {
        cortarPapel();
        printf("QR Code enviado para impressao.\n");
    } else {
        printf("Erro ao imprimir QR Code. Codigo: %d\n", retorno);
    }
}

void imprimirCodigoBarras(void) {
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    retorno = ImpressaoCodigoBarras(8, "{A012345678912", 100, 2, 3);
    if (retorno == 0) {
        cortarPapel();
        printf("Codigo de barras enviado para impressao.\n");
    } else {
        printf("Erro ao imprimir codigo de barras. Codigo: %d\n", retorno);
    }
}

void imprimirXMLSAT(void) {
    char *dadosXML;
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    dadosXML = lerArquivo("XMLSAT.xml");
    if (dadosXML == NULL) {
        printf("Nao foi possivel ler o XMLSAT.xml\n");
        return;
    }

    retorno = ImprimeXMLSAT(dadosXML, 0);
    free(dadosXML);

    if (retorno == 0) {
        cortarPapel();
        printf("XML SAT enviado para impressao.\n");
    } else {
        printf("Erro ao imprimir XML SAT. Codigo: %d\n", retorno);
    }
}

void imprimirXMLCancelamentoSAT(void) {
    char *dadosXML;
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    dadosXML = lerArquivo("CANC_SAT.xml");
    if (dadosXML == NULL) {
        printf("Nao foi possivel ler o CANC_SAT.xml\n");
        return;
    }

    retorno = ImprimeXMLCancelamentoSAT(dadosXML, ASS_QRCODE, 0);
    free(dadosXML);

    if (retorno == 0) {
        cortarPapel();
        printf("XML de cancelamento SAT enviado para impressao.\n");
    } else {
        printf("Erro ao imprimir XML de cancelamento. Codigo: %d\n", retorno);
    }
}

void abrirGavetaElgin(void) {
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    retorno = AbreGavetaElgin();
    if (retorno == 0) {
        printf("Gaveta Elgin aberta com sucesso!\n");
    } else {
        printf("Erro ao abrir gaveta Elgin. Codigo: %d\n", retorno);
    }
}

void abrirGaveta(void) {
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    retorno = AbreGaveta(1, 5, 10);
    if (retorno == 0) {
        printf("Gaveta aberta com sucesso!\n");
    } else {
        printf("Erro ao abrir gaveta. Codigo: %d\n", retorno);
    }
}

void emitirSinalSonoro(void) {
    int retorno;

    if (!exigirConexao()) {
        return;
    }

    retorno = SinalSonoro(1, 1, 1);
    if (retorno == 0) {
        printf("Sinal sonoro emitido!\n");
    } else {
        printf("Erro ao emitir sinal sonoro. Codigo: %d\n", retorno);
    }
}

void exibirMenu(void) {
    printf("\n**********************************************\n");
    printf("**************** MENU IMPRESSORA ****************\n");
    printf("*************************************************\n\n");
    printf("1  - Configurar conexao\n");
    printf("2  - Abrir conexao\n");
    printf("3  - Fechar conexao\n");
    printf("4  - Imprimir texto\n");
    printf("5  - Imprimir QR Code\n");
    printf("6  - Imprimir codigo de barras\n");
    printf("7  - Imprimir XML SAT\n");
    printf("8  - Imprimir XML de cancelamento SAT\n");
    printf("9  - Abrir gaveta (Elgin)\n");
    printf("10 - Abrir gaveta\n");
    printf("11 - Sinal sonoro\n");
    printf("12 - Imprimir copias de teste\n");
    printf("0  - Sair\n");
    printf("--------------------------------------\n");
}

int fazerLogin(void) {
    char usuario[50], senha[50];
    int tentativa;

    for (tentativa = 1; tentativa <= 3; tentativa++) {
        int restantes;

        if (!capturarEntrada("Usuario: ", usuario, sizeof(usuario))
            || !capturarEntrada("Senha: ", senha, sizeof(senha))) {
            return 0;
        }

        if (strcmp(usuario, "admin") == 0 && strcmp(senha, "1234") == 0) {
            printf("Login realizado com sucesso!\n");
            return 1;
        }

        restantes = 3 - tentativa;
        if (restantes > 0) {
            printf("Usuario ou senha incorretos. Tentativas restantes: %d\n", restantes);
        }
    }

    printf("Acesso negado. Numero maximo de tentativas excedido.\n");
    return 0;
}
/* ===================== Programa principal ===================== */

int main(void) {
    if (!carregarDLL()) {
        printf("Programa encerrado.\n");
        return 1;
    }
    if (!fazerLogin()) {
        printf("Acesso negado.\n");
        FreeLibrary(dll);
        return 1;
    }

    for (;;) {
        int opcao;
        exibirMenu();
        if (!lerInteiro("\nDigite a opcao desejada: ", 0, 12, &opcao) || opcao == 0) {
            break;
        }

        switch (opcao) {
            case 1: configurarConexao(); break;
            case 2: abrirConexao(); break;
            case 3: fecharConexao(); break;
            case 4: imprimirTexto(); break;
            case 5: imprimirQRCode(); break;
            case 6: imprimirCodigoBarras(); break;
            case 7: imprimirXMLSAT(); break;
            case 8: imprimirXMLCancelamentoSAT(); break;
            case 9: abrirGavetaElgin(); break;
            case 10: abrirGaveta(); break;
            case 11: emitirSinalSonoro(); break;
            case 12: imprimirCopiasTeste(); break;
        }
    }

    if (conexaoAberta) {
        fecharConexao();
    }
    printf("Saindo do programa...\n");
    FreeLibrary(dll);
    return conexaoAberta || ferror(stdin) ? EXIT_FAILURE : EXIT_SUCCESS;
}
