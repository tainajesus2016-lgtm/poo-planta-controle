#include "arquivos.hpp"
#include "controlador.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string ler_texto(const std::filesystem::path& caminho) {
    std::ifstream arquivo(caminho);
    if (!arquivo.is_open()) {
        throw std::runtime_error("nao foi possivel abrir " + caminho.string());
    }
    std::string conteudo;
    std::getline(arquivo, conteudo, '\0');
    return conteudo;
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

void verificar_leitura_invalida(const std::filesystem::path& pasta,
                                const std::string& documento,
                                const char* mensagem) {
    gravar_texto(pasta / "planta.json", documento);
    Arquivos arquivos(pasta.string());

    bool lancou = false;
    try {
        arquivos.ler_planta();
    } catch (const std::exception&) {
        lancou = true;
    }
    verificar(lancou, mensagem);
}

const char* planta_valida() {
    return R"json({
  "versao": 1,
  "sessao": "teste-autoral",
  "ciclo": 7,
  "timestamp": "2026-10-21T18:00:00Z",
  "leituras": [
    {"tag": "LT-01", "tipo": "nivel", "valor": 48.5, "unidade": "%", "status": "operando"},
    {"tag": "TT-01", "tipo": "temperatura", "valor": 26.2, "unidade": "C", "status": "operando"},
    {"tag": "LT-02", "tipo": "nivel", "valor": null, "unidade": "%", "status": "falha"}
  ],
  "atuadores": {"bomba": 12.5, "aquecedor": 0.0},
  "ultimo_comando": 3,
  "erro_comando": null,
  "motivo_seguro": "sem_arquivo",
  "campo_extra": "ignorado"
})json";
}

int main() {
    const long long identificador =
        std::chrono::system_clock::now().time_since_epoch().count();
    const std::filesystem::path pasta =
        std::filesystem::temp_directory_path() /
        ("autorais_aquisicao_" + std::to_string(identificador));

    try {
        std::filesystem::remove_all(pasta);
        std::filesystem::create_directories(pasta);

        gravar_texto(pasta / "planta.json", planta_valida());
        Arquivos arquivos(pasta.string());

        // 1. Leitura valida: confere os campos convertidos para Aquisicao.
        const Aquisicao aquisicao = arquivos.ler_planta();
        verificar(aquisicao.versao == 1, "versao nao foi lida");
        verificar(aquisicao.sessao == "teste-autoral", "sessao nao foi lida");
        verificar(aquisicao.ciclo == 7, "ciclo nao foi lido");
        verificar(aquisicao.timestamp == "2026-10-21T18:00:00Z",
                  "timestamp nao foi lido");
        verificar(aquisicao.leituras.size() == 3, "quantidade de leituras errada");
        verificar(aquisicao.leituras[0].tag == "LT-01", "tag da primeira leitura");
        verificar(aquisicao.leituras[0].valor.has_value() &&
                      aquisicao.leituras[0].valor.value() == 48.5,
                  "valor da primeira leitura");
        verificar(aquisicao.leituras[2].status == "falha",
                  "status de falha nao foi lido");
        verificar(!aquisicao.leituras[2].valor.has_value(),
                  "valor null deveria continuar sem valor");
        verificar(aquisicao.atuadores.bomba == 12.5, "bomba nao foi lida");
        verificar(aquisicao.atuadores.aquecedor == 0.0, "aquecedor nao foi lido");
        verificar(aquisicao.ultimo_comando == 3, "ultimo_comando nao foi lido");
        verificar(!aquisicao.erro_comando.has_value(),
                  "erro_comando null deveria continuar vazio");
        verificar(aquisicao.motivo_seguro.has_value() &&
                      aquisicao.motivo_seguro.value() == "sem_arquivo",
                  "motivo_seguro nao foi lido");

        // 2. Entradas invalidas devem ser recusadas.
        verificar_leitura_invalida(pasta, "{}",
                                   "objeto vazio deveria ser recusado");
        verificar_leitura_invalida(pasta, "null",
                                   "null deveria ser recusado");
        verificar_leitura_invalida(pasta, "[]",
                                   "lista deveria ser recusada");
        verificar_leitura_invalida(
            pasta,
            R"json({"versao": "1", "sessao": "x", "ciclo": 1, "timestamp": "2026-10-21T18:00:00Z", "leituras": [{"tag": "LT-01", "tipo": "nivel", "valor": 50, "unidade": "%", "status": "operando"}], "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0, "erro_comando": null, "motivo_seguro": null})json",
            "versao em texto deveria ser recusada");
        verificar_leitura_invalida(
            pasta,
            R"json({"versao": 1, "sessao": "x", "ciclo": 1, "timestamp": "21/10/2026 18:00", "leituras": [{"tag": "LT-01", "tipo": "nivel", "valor": 50, "unidade": "%", "status": "operando"}], "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0, "erro_comando": null, "motivo_seguro": null})json",
            "timestamp fora do formato deveria ser recusado");
        verificar_leitura_invalida(
            pasta,
            R"json({"versao": 1, "sessao": "x", "ciclo": 1, "timestamp": "2026-10-21T18:00:00Z", "leituras": [{"tag": "LT-01", "tipo": "nivel", "valor": 50, "unidade": "%", "status": "operando"}, {"tag": "LT-01", "tipo": "nivel", "valor": 60, "unidade": "%", "status": "operando"}], "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0, "erro_comando": null, "motivo_seguro": null})json",
            "tag repetida deveria ser recusada");
        verificar_leitura_invalida(
            pasta,
            R"json({"versao": 1, "sessao": "x", "ciclo": 1, "timestamp": "2026-10-21T18:00:00Z", "leituras": [{"tag": "LT-01", "tipo": "nivel", "valor": null, "unidade": "%", "status": "operando"}], "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0, "erro_comando": null, "motivo_seguro": null})json",
            "valor null com status operando deveria ser recusado");
        verificar_leitura_invalida(
            pasta,
            R"json({"versao": 1, "sessao": "x", "ciclo": 1, "timestamp": "2026-10-21T18:00:00Z", "leituras": [{"tag": "LT-01", "tipo": "nivel", "valor": 50, "unidade": "%", "status": "operando"}], "atuadores": {"bomba": 0, "aquecedor": 0, "extra": 1}, "ultimo_comando": 0, "erro_comando": null, "motivo_seguro": null})json",
            "chave extra em atuadores deveria ser recusada");

        std::filesystem::remove(pasta / "planta.json");
        bool ausente_recusado = false;
        try {
            arquivos.ler_planta();
        } catch (const std::exception&) {
            ausente_recusado = true;
        }
        verificar(ausente_recusado, "arquivo ausente deveria gerar excecao");

        // 3. Gravacao da telemetria: campos preservados, null preservado e alarmes.
        gravar_texto(pasta / "planta.json", planta_valida());
        const Aquisicao para_gravar = arquivos.ler_planta();
        Alarme alarme;
        alarme.codigo = "NIVEL_BAIXO";
        alarme.mensagem = "Nivel abaixo da faixa operacional";
        alarme.severidade = "aviso";
        const std::vector<Alarme> alarmes = {alarme};
        arquivos.gravar_telemetria(para_gravar, alarmes);

        const json telemetria = ler_json(pasta / "telemetria.json");
        verificar(telemetria.at("versao").get<int>() == 1,
                  "telemetria perdeu a versao");
        verificar(telemetria.at("sessao").get<std::string>() == "teste-autoral",
                  "telemetria perdeu a sessao");
        verificar(telemetria.at("ciclo").get<int>() == 7,
                  "telemetria perdeu o ciclo");
        verificar(telemetria.at("timestamp").get<std::string>() ==
                      "2026-10-21T18:00:00Z",
                  "telemetria perdeu o timestamp");
        verificar(telemetria.at("leituras").size() == 3,
                  "telemetria perdeu leituras");
        verificar(telemetria.at("leituras")[2].at("valor").is_null(),
                  "telemetria nao preservou valor null");
        verificar(telemetria.at("atuadores").at("bomba").get<double>() == 12.5,
                  "telemetria perdeu a bomba");
        verificar(telemetria.at("ultimo_comando").get<int>() == 3,
                  "telemetria perdeu ultimo_comando");
        verificar(telemetria.at("erro_comando").is_null(),
                  "telemetria nao preservou erro_comando null");
        verificar(telemetria.at("motivo_seguro").get<std::string>() ==
                      "sem_arquivo",
                  "telemetria perdeu motivo_seguro");
        verificar(telemetria.at("alarmes").is_array() &&
                      telemetria.at("alarmes").size() == 1,
                  "telemetria deve conter os alarmes");
        verificar(telemetria.at("alarmes")[0].at("codigo").get<std::string>() ==
                      "NIVEL_BAIXO",
                  "telemetria perdeu o codigo do alarme");

        // Uma segunda gravacao deve substituir a telemetria anterior.
        arquivos.gravar_telemetria(para_gravar, std::vector<Alarme>());
        const json telemetria_substituida = ler_json(pasta / "telemetria.json");
        verificar(telemetria_substituida.at("alarmes").is_array() &&
                      telemetria_substituida.at("alarmes").empty(),
                  "segunda gravacao nao substituiu a telemetria anterior");

        // 4. Falha de aquisicao nao pode alterar a telemetria anterior.
        const std::string antes = ler_texto(pasta / "telemetria.json");
        gravar_texto(pasta / "planta.json", "{}");
        Controlador controlador(pasta.string());
        const int codigo = controlador.executar(true, 1.0, 0.0);
        verificar(codigo == 3, "entrada invalida deveria retornar 3");
        verificar(ler_texto(pasta / "telemetria.json") == antes,
                  "falha de aquisicao alterou a telemetria anterior");

        std::filesystem::remove_all(pasta);
        std::cout << "OK: leitura, validacao e telemetria preservada\n";
        return 0;
    } catch (const std::exception& erro) {
        std::filesystem::remove_all(pasta);
        std::cerr << "FALHOU: " << erro.what() << '\n';
        return 1;
    }
}
