"""
Given a tree, a list of names that exactly match some leaves of the tree, and a radius, find all leaves that are
within the specified radius of leaves in the list.  Write out a list of those leaves, for use with matUtils extract -s.
Initially I planned to do the pruning in this script, and write out the pruned tree directly, but I kept getting
segmentation faults when removing nodes.
"""

import argparse
import sys

import bte


def read_leaf_set(leaf_list_file: str) -> set[str]:
    """
    Read lines of leaf_list_file into a list and return as a set.
    """
    leaf_list = []
    with open(leaf_list_file, "r") as f:
        for line in f:
            leaf_list.append(line.strip())
    return set(leaf_list)


def init_distances_internal_nodes(node: bte.MATNode, node_distances: dict[str, int], init_val: int):
    """
    Recursively descend from node to add a mapping to node_distances of every internal node name to init_val.
    """
    if node.children:
        node_distances[node.id] = init_val
        for child in node.children:
            init_distances_internal_nodes(child, node_distances, init_val)


def init_distances(tree: bte.MATree, init_val: int) -> dict[str, int]:
    """
    Map every node.id in tree to init_val and return the map.
    """
    # Initialize the leaves in one call to avoid resizing for those at least:
    node_distances = dict.fromkeys(tree.get_leaves_ids(), init_val)
    # Initialize internal node distances
    init_distances_internal_nodes(tree.root, node_distances, init_val)
    return node_distances


def propagate_back(node: bte.MATNode, leaf_set: set[str], node_distances: dict[str, int]) -> int:
    """
    Recursively descend from node; when we get to a leaf, if it is in leaf_set, set its distance to 0.
    Set node_distances on internal nodes on the way back up (minimum of children's distances plus 1).
    """
    if node.children:
        # Internal node's distance is the minimum of its children's distances plus 1
        distance = None
        for child in node.children:
            child_distance = propagate_back(child, leaf_set, node_distances)
            if distance is None or child_distance < distance:
                distance = child_distance
        distance += 1
        node_distances[node.id] = distance
        return distance
    else:
        # It's a leaf -- is it in leaf_set?  If so, distance is 0.
        # Otherwise, node_distances[node.id] has already been initialized with max int, use that for now.
        if node.id in leaf_set:
            node_distances[node.id] = 0
            return 0
        return node_distances[node.id]


def propagate_forward(node: bte.MATNode, node_distances: dict[str, int], radius: int):
    """
    Recursively descend from node, updating child distances to parent's plus 1 if that is less than
    their current distance.  Return the max distance of any descendant.
    """
    if node.children:
        max_child_distance = node_distances[node.id] + 1
        max_descendant_distance = max_child_distance
        for child in node.children:
            if node_distances[child.id] > max_child_distance:
                node_distances[child.id] = max_child_distance
            max_child_descendant_distance = propagate_forward(child, node_distances, radius)
            if max_child_descendant_distance > max_descendant_distance:
                max_descendant_distance = max_child_descendant_distance
        return max_descendant_distance
    return node_distances[node.id]


def prune_to_radius(input_tree_file: str, leaf_list_file: str, radius: int, output_list_file: str):
    """
    Read in tree and leaf list, find leaves within radius of listed leaves, write out list.
    """
    tree = bte.MATree(input_tree_file)
    leaf_set = read_leaf_set(leaf_list_file)
    # Initialize map of nodes, including leaves, to max int
    node_distances = init_distances(tree, sys.maxsize)
    # Propagate distances from leaves in leaf_set back through ancestors
    root_distance = propagate_back(tree.root, leaf_set, node_distances)
    print(f"Root is {root_distance} edges back from nearest leaf in list")
    # Propagate distances from internal nodes forward to descendants
    max_leaf_distance = propagate_forward(tree.root, node_distances, radius)
    print(f"Maximum distance from a leaf in set is {max_leaf_distance} edges")

    # I wanted to use tree.remove_node() with the ID of every leaf with a distance > radius.
    # However, that caused a Segmentation fault, even when I made a list of f"{}" copies of IDs.
    # So write out a list of leaves to retain, and let the caller run matUtils extract -s.
    with open(output_list_file, "w") as f:
        for l in tree.get_leaves_ids():
            if node_distances[l] <= radius:
                print(l, file=f);


def main():
    parser = argparse.ArgumentParser(description="Prune tree to retain leaves within specified radius of leaves in list")
    parser.add_argument('-i', '--input-tree', required=True,
                        help="Input tree file in UShER protobuf format (.pb or .pb.gz)")
    parser.add_argument('-l', '--leaf-list', required=True,
                        help="Input leaf list file: one leaf name per line, exactly matching name used in tree; " +
                        "all of these leaves will be retained, as well as leaves within the specified radius")
    parser.add_argument('-r', '--radius', required=True, type=int,
                        help="Maximum number of edges away from a leaf in the list that a leaf not in the list may be " +
                        "while still being retained in output tree")
    parser.add_argument('-o', '--output-list', required=True,
                        help="Output list of leaves to retain in tree (use matUtils extract -s)")
    args = parser.parse_args()
    prune_to_radius(args.input_tree, args.leaf_list, args.radius, args.output_list)


if __name__ == "__main__":
    main()
