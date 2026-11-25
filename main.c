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
