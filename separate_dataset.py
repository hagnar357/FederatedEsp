import numpy as np
import polars as pl
import os
import sys

def particionar_non_iid_dirichlet(
    labels,
    num_clientes,
    alpha=0.5,
    min_size=20,
    seed=42
):
    """
    Particionamento não-IID via Dirichlet garantindo que cada cliente
    tenha pelo menos min_size amostras.
    """

    rng = np.random.default_rng(seed)

    num_classes = len(np.unique(labels))
    n = len(labels)

    while True:

        dados_clientes = [[] for _ in range(num_clientes)]

        for k in range(num_classes):

            idx_k = np.where(labels == k)[0]
            rng.shuffle(idx_k)

            # Proporções Dirichlet
            proporcoes = rng.dirichlet(
                np.repeat(alpha, num_clientes)
            )

            # Pontos de corte
            cortes = (
                np.cumsum(proporcoes) * len(idx_k)
            ).astype(int)[:-1]

            partes = np.split(idx_k, cortes)

            for cliente, parte in enumerate(partes):
                dados_clientes[cliente].extend(parte.tolist())

        tamanhos = [len(v) for v in dados_clientes]

        if min(tamanhos) >= min_size:
            break

    # Embaralha internamente cada cliente
    for cliente in range(num_clientes):
        rng.shuffle(dados_clientes[cliente])

    return {
        cliente: np.array(indices)
        for cliente, indices in enumerate(dados_clientes)
    }

if __name__ == "__main__":

    num_clientes = int(sys.argv[1])

    # ---------------------------------------------------------
    data = pl.read_csv("FederatedLearningZephyr/data/dataset_full.csv", has_header=False, new_columns=["id", "x1", "x2", "x3", "x4", "y0", "y1", "y2"])
    data = data.drop("id")
    labels_inteiros = np.argmax(data[["y0", "y1", "y2"]], axis=1)
    print(np.unique_counts(labels_inteiros))
    # ---------------------------------------------------------
    # 2. Alimentar as funções de particionamento
    # ---------------------------------------------------------
    # Agora você passa o array 1D para a função
    indices_distribuidos = particionar_non_iid_dirichlet(
        labels=labels_inteiros,
        num_clientes=num_clientes, 
        alpha=0.5
    )

    # ---------------------------------------------------------
    # 2. Preparando a exportação
    # ---------------------------------------------------------
    # Cria um diretório para organizar os arquivos de saída
    diretorio_saida = "datasets"
    os.makedirs(diretorio_saida, exist_ok=True)

    # ---------------------------------------------------------
    # 3. Fatiar, Combinar e Salvar em CSV usando Polars
    # ---------------------------------------------------------
    for cliente_id, indices in indices_distribuidos.items():
        # 3.3 DataFrame: Cria o DataFrame no Polars já aplicando os nomes das colunas
        df_no = pl.DataFrame(data[indices])

        # 3.4 Salvar: Exporta para CSV
        caminho_arquivo = os.path.join(diretorio_saida, f"dataset_no_{cliente_id}.csv")
        df_no.write_csv(caminho_arquivo, include_header=False)

        print(f"✅ Salvo: {caminho_arquivo} | Linhas: {len(df_no)} | Colunas: {len(df_no.columns)}")
        print("Total de classes:\n")
        for classe in ["y0", "y1", "y2"]:
            print(f"\t{classe}: ",sum(df_no[classe]))

    print("\nTodos os datasets particionados foram gerados com sucesso!")