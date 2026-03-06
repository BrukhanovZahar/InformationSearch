"""
Lab 5 — Закон Ципфа.
Строит log-log график распределения термов по частоте
и сравнивает с теоретической кривой Ципфа.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np


def main():
    df = pd.read_csv("freq_data.csv")
    df = df.head(5000)

    ranks = df["rank"].values
    freqs = df["frequency"].values

    fig, ax = plt.subplots(figsize=(11, 7))

    ax.scatter(ranks, freqs, s=4, alpha=0.6, color="#2563eb", label="Корпус (реальные данные)")

    c = freqs[0]
    zipf_theory = c / ranks.astype(float)
    ax.plot(ranks, zipf_theory, color="#dc2626", linewidth=2, linestyle="--",
            label=f"Закон Ципфа (f = {c}/{'{'}r{'}'})")

    ax.set_xscale("log")
    ax.set_yscale("log")

    ax.set_xlabel("Ранг (log)", fontsize=13)
    ax.set_ylabel("Частота (log)", fontsize=13)
    ax.set_title("Распределение термов — закон Ципфа", fontsize=15)
    ax.legend(fontsize=11)
    ax.grid(True, which="both", linestyle=":", alpha=0.4)

    fig.tight_layout()
    fig.savefig("zipf_graph.png", dpi=150)
    print("График сохранён в zipf_graph.png")
    plt.show()


if __name__ == "__main__":
    main()
