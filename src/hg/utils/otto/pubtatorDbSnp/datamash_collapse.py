import sys
from collections import defaultdict
import matplotlib.pyplot as plt


def get_color_gradient(num_items: int) -> tuple[int, int, int]:
    cmap = plt.get_cmap(
        "coolwarm"
    )  # Use 'coolwarm' colormap which transitions from blue to red
    normalized_value = min(
        max((num_items - 1) / 19, 0), 1
    )  # Normalize num_items to [0, 1] range
    rgba = cmap(normalized_value)
    r, g, b, _ = [
        int(255 * v) for v in rgba
    ]  # Convert to 0-255 range and ignore the alpha value
    return r, g, b


def main() -> None:
    x = defaultdict(list)

    for line in sys.stdin:
        line_split = line.split()
        x[line_split[1]].append(line_split[0])

    for a, b in x.items():
        red, green, blue = get_color_gradient(len(b))
        print(f"{a}\t{','.join(b)}\t{len(b)}\t{red},{green},{blue}")


if __name__ == "__main__":
    main()
