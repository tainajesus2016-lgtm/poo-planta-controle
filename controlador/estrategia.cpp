#include "estrategia.hpp"

#include "contrato.hpp"

// Procura uma leitura pela tag. Devolve nullptr quando a tag nao existe.
const Leitura* buscar_leitura(const Aquisicao& aquisicao,
                              const std::string& tag) {
    for (const Leitura& leitura : aquisicao.leituras) {
        if (leitura.tag == tag) {
            return &leitura;
        }
    }
    return nullptr;
}

// A decisao so pode usar leituras presentes, em operacao e com valor.
bool leitura_utilizavel(const Leitura* leitura) {
    if (leitura == nullptr) {
        return false;
    }
    if (leitura->status != "operando") {
        return false;
    }
    if (!leitura->valor.has_value()) {
        return false;
    }
    return true;
}

std::optional<Atuadores> Histerese::decidir(const Aquisicao& aquisicao) {
    const Leitura* nivel = buscar_leitura(aquisicao, "LT-01");
    const Leitura* temperatura = buscar_leitura(aquisicao, "TT-01");

    if (!leitura_utilizavel(nivel) || !leitura_utilizavel(temperatura)) {
        return std::nullopt;
    }

    const double valor_nivel = nivel->valor.value();
    if (valor_nivel <= contrato::nivel_liga) {
        atuais_.bomba = contrato::atuador_max;
    } else if (valor_nivel >= contrato::nivel_desliga) {
        atuais_.bomba = contrato::atuador_min;
    }

    const double valor_temperatura = temperatura->valor.value();
    if (valor_temperatura <= contrato::temperatura_liga) {
        atuais_.aquecedor = contrato::atuador_max;
    } else if (valor_temperatura >= contrato::temperatura_desliga) {
        atuais_.aquecedor = contrato::atuador_min;
    }

    return atuais_;
}
