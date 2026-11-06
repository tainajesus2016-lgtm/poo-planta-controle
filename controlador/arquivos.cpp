#include "arquivos.hpp"

#include "contrato.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using nlohmann::json;

// Funcoes auxiliares de validacao. Elas ficam neste arquivo porque so a
// leitura de planta.json precisa delas.

void exigir_json(bool condicao, const std::string& mensagem) {
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

const json& campo_json(const json& objeto, const char* nome) {
    if (!objeto.contains(nome)) {
        throw std::runtime_error("campo obrigatorio ausente: " +
                                 std::string(nome));
    }
    return objeto.at(nome);
}

int ler_inteiro_json(const json& valor, const std::string& campo) {
    exigir_json(valor.is_number_integer(), campo + " deve ser um inteiro");

    if (valor.is_number_unsigned()) {
        const unsigned long long numero = valor.get<unsigned long long>();
        exigir_json(
            numero <= static_cast<unsigned long long>(std::numeric_limits<int>::max()),
            campo + " esta fora da faixa aceita");
        return static_cast<int>(numero);
    }

    const long long numero = valor.get<long long>();
    exigir_json(
        numero >= static_cast<long long>(std::numeric_limits<int>::min()) &&
            numero <= static_cast<long long>(std::numeric_limits<int>::max()),
        campo + " esta fora da faixa aceita");
    return static_cast<int>(numero);
}

double ler_numero_json(const json& valor, const std::string& campo) {
    exigir_json(valor.is_number(), campo + " deve ser um numero");

    const double numero = valor.get<double>();
    exigir_json(std::isfinite(numero), campo + " deve ser finito");
    return numero;
}

std::string ler_texto_json(const json& valor, const std::string& campo) {
    exigir_json(valor.is_string(), campo + " deve ser um texto");
    return valor.get<std::string>();
}

std::optional<std::string> ler_texto_ou_nulo(const json& valor,
                                             const std::string& campo) {
    if (valor.is_null()) {
        return std::nullopt;
    }
    return ler_texto_json(valor, campo);
}

int ler_dois_digitos(const std::string& texto, std::size_t posicao) {
    return (texto[posicao] - '0') * 10 + (texto[posicao + 1] - '0');
}

bool eh_digito(char caractere) {
    return caractere >= '0' && caractere <= '9';
}

bool eh_bissexto(int ano) {
    return (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
}

int dias_no_mes(int mes, int ano) {
    if (mes == 2) {
        return eh_bissexto(ano) ? 29 : 28;
    }
    if (mes == 4 || mes == 6 || mes == 9 || mes == 11) {
        return 30;
    }
    return 31;
}

void validar_timestamp(const std::string& texto) {
    const std::string formato = "timestamp deve usar o formato AAAA-MM-DDTHH:MM:SSZ";
    exigir_json(texto.size() == 20, formato);

    exigir_json(texto[4] == '-' && texto[7] == '-' && texto[10] == 'T' &&
                    texto[13] == ':' && texto[16] == ':' && texto[19] == 'Z',
                formato);

    for (std::size_t i = 0; i < texto.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19) {
            continue;
        }
        exigir_json(eh_digito(texto[i]), formato);
    }

    const int ano = ler_dois_digitos(texto, 0) * 100 + ler_dois_digitos(texto, 2);
    const int mes = ler_dois_digitos(texto, 5);
    const int dia = ler_dois_digitos(texto, 8);
    const int hora = ler_dois_digitos(texto, 11);
    const int minuto = ler_dois_digitos(texto, 14);
    const int segundo = ler_dois_digitos(texto, 17);

    exigir_json(mes >= 1 && mes <= 12, "timestamp tem mes invalido");
    exigir_json(dia >= 1 && dia <= dias_no_mes(mes, ano),
                "timestamp tem dia invalido");
    exigir_json(hora >= 0 && hora <= 23, "timestamp tem hora invalida");
    exigir_json(minuto >= 0 && minuto <= 59, "timestamp tem minuto invalido");
    exigir_json(segundo >= 0 && segundo <= 59, "timestamp tem segundo invalido");
}

std::optional<std::string> ler_motivo_seguro(const json& valor) {
    const std::optional<std::string> motivo = ler_texto_ou_nulo(valor, "motivo_seguro");
    if (!motivo.has_value()) {
        return std::nullopt;
    }

    const std::string texto = motivo.value();
    exigir_json(texto == "sem_arquivo" || texto == "comando_invalido" ||
                    texto == "comando_expirado" || texto == "sem_comando",
                "motivo_seguro tem valor desconhecido: " + texto);
    return motivo;
}

std::vector<Leitura> ler_leituras(const json& valor) {
    exigir_json(valor.is_array() && !valor.empty(),
                "leituras deve ser uma lista nao vazia");

    std::vector<Leitura> leituras;
    std::vector<std::string> tags;

    for (const json& item : valor) {
        exigir_json(item.is_object(), "cada leitura deve ser um objeto");

        Leitura leitura;
        leitura.tag = ler_texto_json(campo_json(item, "tag"), "tag da leitura");
        exigir_json(!leitura.tag.empty(), "tag da leitura nao pode ser vazia");

        for (const std::string& tag : tags) {
            exigir_json(tag != leitura.tag, "tag repetida em leituras: " + leitura.tag);
        }
        tags.push_back(leitura.tag);

        leitura.tipo = ler_texto_json(campo_json(item, "tipo"), "tipo da leitura");
        exigir_json(leitura.tipo == "nivel" || leitura.tipo == "temperatura",
                    "tipo da leitura desconhecido: " + leitura.tipo);

        leitura.unidade =
            ler_texto_json(campo_json(item, "unidade"), "unidade da leitura");
        const std::string unidade_esperada =
            leitura.tipo == "nivel" ? "%" : "C";
        exigir_json(leitura.unidade == unidade_esperada,
                    "unidade incompativel com o tipo da leitura " + leitura.tag);

        leitura.status =
            ler_texto_json(campo_json(item, "status"), "status da leitura");
        exigir_json(leitura.status == "operando" || leitura.status == "falha",
                    "status da leitura desconhecido: " + leitura.status);

        const json& valor_lido = campo_json(item, "valor");
        if (valor_lido.is_null()) {
            exigir_json(leitura.status == "falha",
                        "valor null so e permitido quando status for falha");
            leitura.valor = std::nullopt;
        } else {
            const double numero = ler_numero_json(valor_lido, "valor da leitura");
            exigir_json(numero >= contrato::leitura_min &&
                            numero <= contrato::leitura_max,
                        "valor da leitura deve estar entre 0 e 100");
            leitura.valor = numero;
        }

        leituras.push_back(leitura);
    }

    return leituras;
}

Atuadores ler_atuadores(const json& valor) {
    exigir_json(valor.is_object(), "atuadores deve ser um objeto");
    exigir_json(valor.size() == 2 && valor.contains("bomba") &&
                    valor.contains("aquecedor"),
                "atuadores deve conter exatamente bomba e aquecedor");

    Atuadores atuadores;
    atuadores.bomba =
        ler_numero_json(campo_json(valor, "bomba"), "atuador bomba");
    atuadores.aquecedor =
        ler_numero_json(campo_json(valor, "aquecedor"), "atuador aquecedor");

    exigir_json(atuadores.bomba >= contrato::atuador_min &&
                    atuadores.bomba <= contrato::atuador_max,
                "atuador bomba deve estar entre 0 e 100");
    exigir_json(atuadores.aquecedor >= contrato::atuador_min &&
                    atuadores.aquecedor <= contrato::atuador_max,
                "atuador aquecedor deve estar entre 0 e 100");

    return atuadores;
}

// Grava um JSON em um temporario e depois substitui o destino por renomeacao.
// Assim nenhum leitor ve um arquivo pela metade.
void gravar_json_atomico(const std::string& destino, const json& documento) {
    const std::string temporario = destino + ".tmp";

    {
        std::ofstream arquivo(temporario);
        if (!arquivo.is_open()) {
            throw std::runtime_error("nao foi possivel criar " + temporario);
        }

        arquivo << documento.dump(2) << '\n';
        arquivo.flush();
        if (!arquivo) {
            arquivo.close();
            std::filesystem::remove(temporario);
            throw std::runtime_error("falha ao gravar " + temporario);
        }
    }

    std::error_code erro;
    std::filesystem::rename(temporario, destino, erro);
    if (erro) {
        std::filesystem::remove(temporario);
        throw std::runtime_error("nao foi possivel substituir " + destino + ": " +
                                 erro.message());
    }
}

Arquivos::Arquivos(std::string pasta)
    : pasta_(pasta) {}

std::string Arquivos::caminho(const std::string& nome) const {
    if (pasta_.empty()) {
        return nome;
    }
    return pasta_ + "/" + nome;
}

Aquisicao Arquivos::ler_planta() const {
    const std::string nome = caminho("planta.json");

    std::ifstream arquivo(nome);
    if (!arquivo.is_open()) {
        throw std::runtime_error("nao foi possivel abrir " + nome);
    }

    json documento;
    try {
        arquivo >> documento;
    } catch (const json::exception& erro) {
        throw std::runtime_error("JSON invalido em " + nome + ": " + erro.what());
    }

    exigir_json(documento.is_object(), nome + " deve conter um objeto JSON");
    exigir_json(!documento.empty(), nome + " nao pode ser um objeto vazio");

    Aquisicao aquisicao;
    aquisicao.versao =
        ler_inteiro_json(campo_json(documento, "versao"), "versao");
    exigir_json(aquisicao.versao == contrato::versao, "versao deve ser 1");

    aquisicao.sessao = ler_texto_json(campo_json(documento, "sessao"), "sessao");
    exigir_json(!aquisicao.sessao.empty(), "sessao nao pode ser vazia");

    aquisicao.ciclo = ler_inteiro_json(campo_json(documento, "ciclo"), "ciclo");
    exigir_json(aquisicao.ciclo > 0, "ciclo deve ser positivo");

    aquisicao.timestamp =
        ler_texto_json(campo_json(documento, "timestamp"), "timestamp");
    validar_timestamp(aquisicao.timestamp);

    aquisicao.leituras = ler_leituras(campo_json(documento, "leituras"));
    aquisicao.atuadores = ler_atuadores(campo_json(documento, "atuadores"));

    aquisicao.ultimo_comando =
        ler_inteiro_json(campo_json(documento, "ultimo_comando"), "ultimo_comando");
    exigir_json(aquisicao.ultimo_comando >= 0,
                "ultimo_comando nao pode ser negativo");

    aquisicao.erro_comando =
        ler_texto_ou_nulo(campo_json(documento, "erro_comando"), "erro_comando");
    aquisicao.motivo_seguro =
        ler_motivo_seguro(campo_json(documento, "motivo_seguro"));

    return aquisicao;
}

void Arquivos::gravar_telemetria(const Aquisicao& aquisicao,
                                 const std::vector<Alarme>& alarmes) const {
    json documento;
    documento["versao"] = aquisicao.versao;
    documento["sessao"] = aquisicao.sessao;
    documento["ciclo"] = aquisicao.ciclo;
    documento["timestamp"] = aquisicao.timestamp;

    json leituras = json::array();
    for (const Leitura& leitura : aquisicao.leituras) {
        json item;
        item["tag"] = leitura.tag;
        item["tipo"] = leitura.tipo;
        if (leitura.valor.has_value()) {
            item["valor"] = leitura.valor.value();
        } else {
            item["valor"] = nullptr;
        }
        item["unidade"] = leitura.unidade;
        item["status"] = leitura.status;
        leituras.push_back(item);
    }
    documento["leituras"] = leituras;

    json atuadores;
    atuadores["bomba"] = aquisicao.atuadores.bomba;
    atuadores["aquecedor"] = aquisicao.atuadores.aquecedor;
    documento["atuadores"] = atuadores;

    documento["ultimo_comando"] = aquisicao.ultimo_comando;

    if (aquisicao.erro_comando.has_value()) {
        documento["erro_comando"] = aquisicao.erro_comando.value();
    } else {
        documento["erro_comando"] = nullptr;
    }

    if (aquisicao.motivo_seguro.has_value()) {
        documento["motivo_seguro"] = aquisicao.motivo_seguro.value();
    } else {
        documento["motivo_seguro"] = nullptr;
    }

    json lista_alarmes = json::array();
    for (const Alarme& alarme : alarmes) {
        json item;
        item["codigo"] = alarme.codigo;
        item["mensagem"] = alarme.mensagem;
        item["severidade"] = alarme.severidade;
        lista_alarmes.push_back(item);
    }
    documento["alarmes"] = lista_alarmes;

    gravar_json_atomico(caminho("telemetria.json"), documento);
}

void Arquivos::gravar_comando(const Comando& comando) const {
    json documento;
    documento["versao"] = comando.versao;
    documento["sessao"] = comando.sessao;
    documento["id"] = comando.id;

    json atuadores;
    atuadores["bomba"] = comando.atuadores.bomba;
    atuadores["aquecedor"] = comando.atuadores.aquecedor;
    documento["atuadores"] = atuadores;

    gravar_json_atomico(caminho("comando_planta.json"), documento);
}

EstadoControlador Arquivos::carregar_estado() const {
    const std::string nome = caminho("controlador_estado.json");

    std::ifstream arquivo(nome);
    if (!arquivo.is_open()) {
        return EstadoControlador();
    }

    try {
        json documento;
        arquivo >> documento;

        exigir_json(documento.is_object() && !documento.empty(),
                    nome + " deve conter um objeto JSON");

        EstadoControlador estado;
        estado.versao =
            ler_inteiro_json(campo_json(documento, "versao"), "versao do estado");
        exigir_json(estado.versao == contrato::versao,
                    "versao do estado deve ser 1");

        estado.sessao =
            ler_texto_json(campo_json(documento, "sessao"), "sessao do estado");
        exigir_json(!estado.sessao.empty(), "sessao do estado nao pode ser vazia");

        estado.ultimo_ciclo_processado =
            ler_inteiro_json(campo_json(documento, "ultimo_ciclo_processado"),
                             "ultimo_ciclo_processado");
        exigir_json(estado.ultimo_ciclo_processado >= 0,
                    "ultimo_ciclo_processado nao pode ser negativo");

        estado.ultimo_id_confirmado =
            ler_inteiro_json(campo_json(documento, "ultimo_id_confirmado"),
                             "ultimo_id_confirmado");
        exigir_json(estado.ultimo_id_confirmado >= 0,
                    "ultimo_id_confirmado nao pode ser negativo");

        const json& id_pendente = campo_json(documento, "id_pendente");
        if (!id_pendente.is_null()) {
            estado.id_pendente =
                ler_inteiro_json(id_pendente, "id_pendente do estado");
        }

        estado.ciclo_emissao =
            ler_inteiro_json(campo_json(documento, "ciclo_emissao"),
                             "ciclo_emissao");
        exigir_json(estado.ciclo_emissao >= 0,
                    "ciclo_emissao nao pode ser negativo");

        const json& atuadores = campo_json(documento, "atuadores_pendentes");
        if (!atuadores.is_null()) {
            estado.atuadores_pendentes = ler_atuadores(atuadores);
        }

        if (estado.id_pendente.has_value()) {
            exigir_json(estado.id_pendente.value() > 0,
                        "id_pendente deve ser positivo");
            exigir_json(estado.atuadores_pendentes.has_value(),
                        "id_pendente exige atuadores_pendentes");
            exigir_json(estado.ciclo_emissao > 0,
                        "id_pendente exige ciclo_emissao positivo");
        } else {
            exigir_json(!estado.atuadores_pendentes.has_value(),
                        "atuadores_pendentes exige id_pendente");
            exigir_json(estado.ciclo_emissao == 0,
                        "sem id_pendente, ciclo_emissao deve ser 0");
        }

        return estado;
    } catch (const std::exception&) {
        // Estado ausente, ilegivel ou corrompido: primeira execucao.
        return EstadoControlador();
    }
}

void Arquivos::salvar_estado(const EstadoControlador& estado) const {
    json documento;
    documento["versao"] = estado.versao;
    documento["sessao"] = estado.sessao;
    documento["ultimo_ciclo_processado"] = estado.ultimo_ciclo_processado;
    documento["ultimo_id_confirmado"] = estado.ultimo_id_confirmado;

    if (estado.id_pendente.has_value()) {
        documento["id_pendente"] = estado.id_pendente.value();
    } else {
        documento["id_pendente"] = nullptr;
    }

    documento["ciclo_emissao"] = estado.ciclo_emissao;

    if (estado.atuadores_pendentes.has_value()) {
        json atuadores;
        atuadores["bomba"] = estado.atuadores_pendentes.value().bomba;
        atuadores["aquecedor"] = estado.atuadores_pendentes.value().aquecedor;
        documento["atuadores_pendentes"] = atuadores;
    } else {
        documento["atuadores_pendentes"] = nullptr;
    }

    gravar_json_atomico(caminho("controlador_estado.json"), documento);
}
