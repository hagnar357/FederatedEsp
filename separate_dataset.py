import numpy as np
import polars as pl
import os
import sys

# must match the student/global model's input/output widths (see setFederatedLearningGlobalModel
# in FederatedLearningServer/src/federatedlearning.c)
NUM_FEATURES = 32
NUM_CLASSES = 6

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
    teacher_node_part = float(sys.argv[2]) if len(sys.argv) > 2 else None

    # ---------------------------------------------------------
    feature_cols = [f"x{i}" for i in range(1, NUM_FEATURES + 1)]
    label_cols = [f"y{i}" for i in range(NUM_CLASSES)]
    data = pl.read_csv("FederatedLearningZephyr/data/dataset_full.csv", has_header=False,
                       new_columns=["id"] + feature_cols + label_cols)
    data = data.drop("id")
    labels_inteiros = np.argmax(data[label_cols], axis=1)
    print(np.unique_counts(labels_inteiros))
    # ---------------------------------------------------------
    # 2. Alimentar as funções de particionamento
    # ---------------------------------------------------------
    # Agora você passa o array 1D para a função

    # tirando um dataset balanceado para o teacher
    labels_inteiros = np.asarray(labels_inteiros)
    indices_restantes = np.arange(len(labels_inteiros))
    indices_teacher = None

    if teacher_node_part is not None:
        if not 0 < teacher_node_part < 1:
            raise ValueError("A partição do teacher deve ser um float entre 0 e 1")

        num_clientes -= 1  # o teacher ocupa um dos nós (índice 0)
        rng_teacher = np.random.default_rng(42)
        classes = np.unique(labels_inteiros)
        menor_classe = min(int(np.sum(labels_inteiros == k)) for k in classes)

        # Mesmo número de amostras por classe (balanceado), limitado pela menor classe
        por_classe = min(int(teacher_node_part * len(labels_inteiros) / len(classes)), menor_classe)
        if por_classe < 1:
            raise ValueError("Partição do teacher muito pequena para gerar amostras balanceadas")

        escolhidos = []
        for k in classes:
            idx_k = np.where(labels_inteiros == k)[0]
            escolhidos.extend(rng_teacher.choice(idx_k, size=por_classe, replace=False).tolist())

        indices_teacher = np.array(escolhidos)
        rng_teacher.shuffle(indices_teacher)

        # Remove as amostras do teacher antes da distribuição Dirichlet
        indices_restantes = np.setdiff1d(indices_restantes, indices_teacher)

    # Dirichlet sobre o restante; converte índices locais de volta para os globais
    particoes = particionar_non_iid_dirichlet(
        labels=labels_inteiros[indices_restantes],
        num_clientes=num_clientes,
        alpha=0.5
    )

    deslocamento = 1 if indices_teacher is not None else 0
    indices_distribuidos = {}
    if indices_teacher is not None:
        indices_distribuidos[0] = indices_teacher
    for cliente_id, indices in particoes.items():
        indices_distribuidos[cliente_id + deslocamento] = indices_restantes[indices]

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
        for classe in label_cols:
            print(f"\t{classe}: ",sum(df_no[classe]))

    print("\nTodos os datasets particionados foram gerados com sucesso!")