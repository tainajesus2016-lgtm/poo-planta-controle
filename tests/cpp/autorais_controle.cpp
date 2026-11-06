#include "arquivos.hpp"
#include "contrato.hpp"
#include "controlador.hpp"
#include "estrategia.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "nlohmann/json.hpp"

using nlohmann::json;

void verificar(bool condicao, const char* mensagem) {
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

void gravar_texto(const std::filesystem::path& caminho,
                  const std::string& conteudo) {
    std::ofstream arquivo(caminho);
    if (!arquivo.is_open()) {
        throw std::runtime_error("nao foi possivel criar " + caminho.string());
    }
    arquivo << conteudo;
}

void gravar_json(const std::filesystem::path& caminho, const json& documento) {
    gravar_texto(caminho, documento.dump(2) + "\n");
}

json ler_json(const std::filesystem::path& caminho) {
    std::ifstream arquivo(caminho);
    if (!arquivo.is_open()) {
        throw std::runtime_error("nao foi possivel abrir " + caminho.string());
    }
    json documento;
    arquivo >> documento;
    return documento;
}

std::filesystem::path criar_pasta(const std::filesystem::path& raiz,
                                  const std::string& nome) {
    const std::filesystem::path pasta = raiz / nome;
    std::filesystem::remove_all(pasta);
    std::filesystem::create_directories(pasta);
    return pasta;
}

Leitura criar_leitura(const std::string& tag, const std::string& tipo,
                      double valor, const std::string& unidade) {
    Leitura leitura;
    leitura.tag = tag;
    leitura.tipo = tipo;
    leitura.valor = valor;
    leitura.unidade = unidade;
    leitura.status = "operando";
    return leitura;
}

Aquisicao criar_aquisicao(double nivel, double temperatura) {
    Aquisicao aquisicao;
    aquisicao.versao = contrato::versao;
    aquisicao.sessao = "sessao-teste";
    aquisicao.ciclo = 1;
    aquisicao.timestamp = "2999-01-01T00:00:00Z";
    aquisicao.leituras.push_back(criar_leitura("LT-01", "nivel", nivel, "%"));
    aquisicao.leituras.push_back(
        criar_leitura("TT-01", "temperatura", temperatura, "C"));
    aquisicao.ultimo_comando = 0;
    return aquisicao;
}

void verificar_decisao(const std::optional<Atuadores>& decisao, double bomba,
                       double aquecedor, const char* mensagem) {
    verificar(decisao.has_value(), mensagem);
    verificar(decisao.value().bomba == bomba, mensagem);
    verificar(decisao.value().aquecedor == aquecedor, mensagem);
}

void testar_histerese() {
    Histerese histerese;

    verificar_decisao(histerese.decidir(criar_aquisicao(45.0, 24.0)), 100.0,
                      100.0, "limites inferiores devem ligar os atuadores");
    verificar_decisao(histerese.decidir(criar_aquisicao(55.0, 28.0)), 0.0,
                      0.0, "limites superiores devem desligar os atuadores");
    verificar_decisao(histerese.decidir(criar_aquisicao(50.0, 26.0)), 0.0,
                      0.0, "faixa intermediaria deve manter o estado inicial");

    verificar_decisao(histerese.decidir(criar_aquisicao(40.0, 20.0)), 100.0,
                      100.0, "valores baixos devem ligar os atuadores");
    verificar_decisao(histerese.decidir(criar_aquisicao(50.0, 26.0)), 100.0,
                      100.0, "faixa intermediaria deve manter os atuadores ligados");

    Aquisicao sem_temperatura = criar_aquisicao(40.0, 20.0);
    sem_temperatura.leituras.resize(1);
    verificar(!histerese.decidir(sem_temperatura).has_value(),
              "leitura ausente deve impedir a decisao");
    verificar_decisao(histerese.decidir(criar_aquisicao(50.0, 26.0)), 100.0,
                      100.0, "recusa por leitura ausente deve preservar o estado");

    Aquisicao com_falha = criar_aquisicao(40.0, 20.0);
    com_falha.leituras[1].status = "falha";
    com_falha.leituras[1].valor = std::nullopt;
    verificar(!histerese.decidir(com_falha).has_value(),
              "leitura em falha deve impedir a decisao");
    verificar_decisao(histerese.decidir(criar_aquisicao(50.0, 26.0)), 100.0,
                      100.0, "recusa por leitura em falha deve preservar o estado");
}

void testar_estado(const std::filesystem::path& pasta) {
    Arquivos arquivos(pasta.string());

    const EstadoControlador vazio = arquivos.carregar_estado();
    verificar(vazio.sessao.empty(), "estado ausente deveria ser vazio");
    verificar(vazio.ultimo_ciclo_processado == 0,
              "estado ausente deveria ter ciclo zero");
    verificar(!vazio.id_pendente.has_value(),
              "estado ausente nao deveria ter id pendente");

    gravar_texto(pasta / "controlador_estado.json", "{");
    verificar(arquivos.carregar_estado().sessao.empty(),
              "estado com JSON invalido deveria ser vazio");

    gravar_texto(pasta / "controlador_estado.json", "{}");
    verificar(arquivos.carregar_estado().sessao.empty(),
              "estado sem campos deveria ser vazio");

    EstadoControlador estado;
    estado.sessao = "sessao-teste";
    estado.ultimo_ciclo_processado = 12;
    estado.ultimo_id_confirmado = 3;
    estado.id_pendente = 4;
    estado.ciclo_emissao = 12;
    Atuadores pendentes;
    pendentes.bomba = 100.0;
    pendentes.aquecedor = 0.0;
    estado.atuadores_pendentes = pendentes;

    arquivos.salvar_estado(estado);
    const EstadoControlador lido = arquivos.carregar_estado();
    verificar(lido.sessao == "sessao-teste", "estado perdeu a sessao");
    verificar(lido.ultimo_ciclo_processado == 12,
              "estado perdeu o ultimo ciclo");
    verificar(lido.ultimo_id_confirmado == 3,
              "estado perdeu o id confirmado");
    verificar(lido.id_pendente.has_value() && lido.id_pendente.value() == 4,
              "estado perdeu o id pendente");
    verificar(lido.ciclo_emissao == 12, "estado perdeu o ciclo de emissao");
    verificar(lido.atuadores_pendentes.has_value() &&
                  lido.atuadores_pendentes.value().bomba == 100.0 &&
                  lido.atuadores_pendentes.value().aquecedor == 0.0,
              "estado perdeu os atuadores pendentes");

    estado.id_pendente = std::nullopt;
    estado.ciclo_emissao = 0;
    estado.atuadores_pendentes = std::nullopt;
    arquivos.salvar_estado(estado);

    const json documento = ler_json(pasta / "controlador_estado.json");
    verificar(documento.at("id_pendente").is_null(),
              "estado sem pendente deve gravar id_pendente null");
    verificar(documento.at("atuadores_pendentes").is_null(),
              "estado sem pendente deve gravar atuadores_pendentes null");
    verificar(documento.at("ciclo_emissao").get<int>() == 0,
              "estado sem pendente deve gravar ciclo_emissao zero");
}

void testar_comando(const std::filesystem::path& pasta) {
    Arquivos arquivos(pasta.string());

    Comando comando;
    comando.versao = contrato::versao;
    comando.sessao = "sessao-teste";
    comando.id = 7;
    comando.atuadores.bomba = 100.0;
    comando.atuadores.aquecedor = 0.0;

    arquivos.gravar_comando(comando);

    const json documento = ler_json(pasta / "comando_planta.json");
    verificar(documento.at("versao").get<int>() == contrato::versao,
              "comando perdeu a versao");
    verificar(documento.at("sessao").get<std::string>() == "sessao-teste",
              "comando perdeu a sessao");
    verificar(documento.at("id").get<int>() == 7, "comando perdeu o id");
    verificar(documento.at("atuadores").size() == 2u,
              "comando deve ter exatamente dois atuadores");
    verificar(documento.at("atuadores").at("bomba").get<double>() == 100.0,
              "comando perdeu a bomba");
    verificar(documento.at("atuadores").at("aquecedor").get<double>() == 0.0,
              "comando perdeu o aquecedor");
}

json criar_planta_json(const std::string& sessao, int ciclo, int ultimo_comando,
                       double nivel, double temperatura) {
    json documento;
    documento["versao"] = contrato::versao;
    documento["sessao"] = sessao;
    documento["ciclo"] = ciclo;
    documento["timestamp"] = "2999-01-01T00:00:00Z";

    json leituras = json::array();
    json nivel_json;
    nivel_json["tag"] = "LT-01";
    nivel_json["tipo"] = "nivel";
    nivel_json["valor"] = nivel;
    nivel_json["unidade"] = "%";
    nivel_json["status"] = "operando";
    leituras.push_back(nivel_json);

    json temperatura_json;
    temperatura_json["tag"] = "TT-01";
    temperatura_json["tipo"] = "temperatura";
    temperatura_json["valor"] = temperatura;
    temperatura_json["unidade"] = "C";
    temperatura_json["status"] = "operando";
    leituras.push_back(temperatura_json);

    documento["leituras"] = leituras;

    json atuadores;
    atuadores["bomba"] = 0.0;
    atuadores["aquecedor"] = 0.0;
    documento["atuadores"] = atuadores;

    documento["ultimo_comando"] = ultimo_comando;
    documento["erro_comando"] = nullptr;
    documento["motivo_seguro"] = nullptr;
    return documento;
}

void testar_controlador(const std::filesystem::path& raiz) {
    const std::filesystem::path pasta = criar_pasta(raiz, "controlador");
    gravar_json(pasta / "planta.json",
                criar_planta_json("sessao-ctrl", 1, 0, 40.0, 20.0));

    Controlador controlador(pasta.string());
    verificar(controlador.executar(true, 1.0, 10.0) == 0,
              "primeira execucao deveria retornar 0");

    const json primeiro = ler_json(pasta / "comando_planta.json");
    verificar(primeiro.at("id").get<int>() == 1,
              "primeiro comando deveria ter id 1");
    verificar(primeiro.at("sessao").get<std::string>() == "sessao-ctrl",
              "comando deveria usar a sessao da aquisicao");
    verificar(primeiro.at("atuadores").at("bomba").get<double>() == 100.0,
              "nivel baixo deveria ligar a bomba");
    verificar(primeiro.at("atuadores").at("aquecedor").get<double>() == 100.0,
              "temperatura baixa deveria ligar o aquecedor");

    // Reprocessar a mesma aquisicao nao pode mudar o comando.
    verificar(controlador.executar(true, 1.0, 10.0) == 0,
              "reprocessamento deveria retornar 0");
    const json repetido = ler_json(pasta / "comando_planta.json");
    verificar(repetido == primeiro,
              "reprocessar o mesmo ciclo nao pode mudar o comando");

    // A planta confirma o id 1 e entrega um ciclo novo.
    gravar_json(pasta / "planta.json",
                criar_planta_json("sessao-ctrl", 2, 1, 50.0, 26.0));
    verificar(controlador.executar(true, 1.0, 10.0) == 0,
              "ciclo novo deveria retornar 0");
    const json confirmado = ler_json(pasta / "comando_planta.json");
    verificar(confirmado.at("id").get<int>() == 2,
              "depois da confirmacao, o proximo id deveria ser 2");

    // Sessao nova reinicia a numeracao pela planta.
    gravar_json(pasta / "planta.json",
                criar_planta_json("sessao-nova", 1, 0, 40.0, 20.0));
    verificar(controlador.executar(true, 1.0, 10.0) == 0,
              "sessao nova deveria retornar 0");
    const json nova_sessao = ler_json(pasta / "comando_planta.json");
    verificar(nova_sessao.at("id").get<int>() == 1,
              "sessao nova deveria reiniciar o id em 1");
    verificar(nova_sessao.at("sessao").get<std::string>() == "sessao-nova",
              "comando deveria usar a nova sessao");
}

void testar_expiracao(const std::filesystem::path& raiz) {
    const std::filesystem::path pasta = criar_pasta(raiz, "expiracao");
    Arquivos arquivos(pasta.string());

    EstadoControlador estado;
    estado.sessao = "sessao-exp";
    estado.ultimo_ciclo_processado = 1;
    estado.ultimo_id_confirmado = 0;
    estado.id_pendente = 9;
    estado.ciclo_emissao = 1;
    Atuadores pendentes;
    pendentes.bomba = 100.0;
    pendentes.aquecedor = 0.0;
    estado.atuadores_pendentes = pendentes;
    arquivos.salvar_estado(estado);

    gravar_json(pasta / "planta.json",
                criar_planta_json("sessao-exp", 6, 0, 40.0, 20.0));

    Controlador controlador(pasta.string());
    verificar(controlador.executar(true, 1.0, 10.0) == 0,
              "expiracao deveria retornar 0");

    const json comando = ler_json(pasta / "comando_planta.json");
    verificar(comando.at("id").get<int>() == 1,
              "id expirado deveria permitir um novo comando");
}

void testar_sem_comando(const std::filesystem::path& raiz) {
    // Dados obsoletos: publica telemetria, mas nao publica comando.
    const std::filesystem::path obsoleto = criar_pasta(raiz, "obsoleto");
    json planta = criar_planta_json("sessao-obsoleta", 1, 0, 40.0, 20.0);
    planta["timestamp"] = "2000-01-01T00:00:00Z";
    gravar_json(obsoleto / "planta.json", planta);

    Controlador controlador_obsoleto(obsoleto.string());
    verificar(controlador_obsoleto.executar(true, 1.0, 10.0) == 0,
              "dados obsoletos deveriam retornar 0");
    verificar(std::filesystem::exists(obsoleto / "telemetria.json"),
              "dados obsoletos ainda devem gerar telemetria");
    verificar(!std::filesystem::exists(obsoleto / "comando_planta.json"),
              "dados obsoletos nao devem gerar comando");

    // Leitura em falha: tambem nao pode gerar comando.
    const std::filesystem::path falha = criar_pasta(raiz, "falha");
    planta = criar_planta_json("sessao-falha", 1, 0, 40.0, 20.0);
    planta["leituras"][1]["status"] = "falha";
    planta["leituras"][1]["valor"] = nullptr;
    gravar_json(falha / "planta.json", planta);

    Controlador controlador_falha(falha.string());
    verificar(controlador_falha.executar(true, 1.0, 10.0) == 0,
              "leitura em falha deveria retornar 0");
    verificar(std::filesystem::exists(falha / "telemetria.json"),
              "leitura em falha ainda deve gerar telemetria");
    verificar(!std::filesystem::exists(falha / "comando_planta.json"),
              "leitura em falha nao deve gerar comando");

    // Entrada invalida: nao pode sobrescrever a telemetria anterior.
    const std::filesystem::path invalida = criar_pasta(raiz, "invalida");
    gravar_texto(invalida / "planta.json", "{}");
    json anterior;
    anterior["anterior"] = true;
    gravar_json(invalida / "telemetria.json", anterior);

    Controlador controlador_invalido(invalida.string());
    verificar(controlador_invalido.executar(true, 1.0, 10.0) == 3,
              "entrada invalida deveria retornar 3");
    verificar(ler_json(invalida / "telemetria.json") == anterior,
              "entrada invalida nao pode sobrescrever a telemetria");
    verificar(!std::filesystem::exists(invalida / "comando_planta.json"),
              "entrada invalida nao pode gerar comando");
}

int main() {
    const long long identificador =
        std::chrono::system_clock::now().time_since_epoch().count();
    const std::filesystem::path raiz =
        std::filesystem::temp_directory_path() /
        ("autorais_controle_" + std::to_string(identificador));

    try {
        std::filesystem::remove_all(raiz);
        std::filesystem::create_directories(raiz);

        testar_histerese();
        testar_estado(criar_pasta(raiz, "estado"));
        testar_comando(criar_pasta(raiz, "comando"));
        testar_controlador(raiz);
        testar_expiracao(raiz);
        testar_sem_comando(raiz);

        std::filesystem::remove_all(raiz);
        std::cout << "OK: histerese, estado, comando e reconciliacao\n";
        return 0;
    } catch (const std::exception& erro) {
        std::filesystem::remove_all(raiz);
        std::cerr << "FALHOU: " << erro.what() << '\n';
        return 1;
    }
}
