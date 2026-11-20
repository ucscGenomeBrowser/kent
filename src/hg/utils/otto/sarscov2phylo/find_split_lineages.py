"""
Search a tree for lineage nodes that have niece nodes with the same mutation(s), which are often split-off pieces of
the lineage that should be rejoined.  Just report them instead of fixing them because sometimes they might be true
recurring mutations, such as a Spike mutation that confers an advantage.
"""

import argparse
import bte

default_min_descendants = 10
default_min_child_proportion = 0.75


def annotation_to_string(node: bte.MATNode) -> str:
    """
    Return node.annotations as a comma-separated list of non-empty components, or empty string if no annotations.
    """
    if node.annotations:
        return ', '.join([a for a in node.annotations if a])
    return ''


def count_descendants_store_lineages(node: bte.MATNode, counts: dict[str, int], node_lineages: dict[str, str]) -> int:
    """
    Recursively decend from node to count its leaf descendants and build up dict of counts as we go.
    Also build up a dict mapping node IDs to lineage annotations (only annotated nodes, not all nodes).
    """
    count = 0 if node.children else 1
    for child in node.children:
        count += count_descendants_store_lineages(child, counts, node_lineages)
    if node.children:
        counts[node.id] = count
    annotation = annotation_to_string(node)
    if annotation:
        node_lineages[node.id] = annotation
    return count


def find_split_lineages_r(node: bte.MATNode) -> list[tuple[str, bte.MATNode, bte.MATNode]]:
    """
    Recursively descend from node.  If node is annotated as the start of a lineage, search each of node's siblings
    for children with the same mutations.  If any are found, return a list of them as tuples of
    [lineage, node, niece_node].
    """
    split_lineages = []
    lineage = annotation_to_string(node)
    if lineage:
        if node.parent:
            lineage_mutations = set(node.mutations)
            siblings = [sib for sib in node.parent.children if sib.id.startswith("node_") and sib != node]
            for sibling in siblings:
                for child in sibling.children:
                    if child.children and set(child.mutations).issuperset(lineage_mutations):
                        split_lineages.append((lineage, node, child))
    for child in node.children:
        split_lineages += find_split_lineages_r(child)
    return split_lineages


def seqs_to_prune(node, exclude_node):
    """
    Return a list of leaf descendants of node excluding exclude_node's leaf descendants.
    These are the sequences to prune (sparing the child_node branch) before optimization to get
    the child_node branch moved into the desired lineage branch.  The pruned sequences can be
    added back in the next daily build, and if they have an N or ambiguous base at the position
    of the lineage-defining mutation then they might have it inferred and end up on the lineage
    branch too.
    """
    if node.id == exclude_node.id:
        return [];
    else:
        non_child_leaves = []
        if node.children:
            for child in node.children:
                non_child_leaves += seqs_to_prune(child, exclude_node)
        else:
            non_child_leaves.append(node.id)
        return non_child_leaves


def report_splits(split_lineages: list[tuple[str, bte.MATNode, bte.MATNode]], min_descendants: int,
                  min_child_proportion: float, counts: dict[str, int], node_lineages: dict[str, str],
                  exempt: set[str], prune_file: str):
    """
    List potential split-lineage nodes and their properties.
    """
    prune_list = []
    for (lineage, lineage_node, split_node) in split_lineages:
        if counts[split_node.id] >= min_descendants:
            sibling = split_node.parent
            sibling_annotation = annotation_to_string(sibling)
            split_node_annotation = annotation_to_string(split_node)
            child_proportion = counts[split_node.id] / counts[sibling.id]
            description = (f"{lineage} {lineage_node.id} " +
                           f"sibling {sibling.id} {sibling_annotation} " +
                           f"({' '.join(sibling.mutations)}, {counts[sibling.id]}) " +
                           f"child {split_node.id} ({counts[split_node.id]}, {child_proportion:.3f}) " +
                           f"has mutations {' '.join(split_node.mutations)}, " +
                           f"superset of lineage mutations {' '.join(lineage_node.mutations)}")
            if sibling_annotation:
                description += f" -- but sibling is annotated as {sibling_annotation}"
                if split_node_annotation:
                    description += f" and child is annotated as {split_node_annotation}"
            elif split_node_annotation:
                description += f" -- but child is annotated as {split_node_annotation}"
            elif child_proportion >= min_child_proportion:
                prune_count = counts[sibling.id] - counts[split_node.id]
                if prune_count > 1 and exempt.issuperset(set(lineage_node.mutations)):
                    description += f" -- but {', '.join(lineage_node.mutations)} is exempt"
                else:
                    prune_proportion = prune_count / counts[sibling.id]
                    description += f" --> prune {prune_count} ({prune_proportion:.3f})"
                    prune_list += seqs_to_prune(sibling, split_node)
            print(description)
    if prune_file:
        print(f"Writing {len(prune_list)} seqs to {prune_file}.")
        with open(prune_file, "w") as pf:
            for seq in prune_list:
                print(seq, file=pf)


def find_split_lineages(input_tree_file: str, min_descendants: int, min_child_proportion: float, exempt: set[str],
                        prune_file: str):
    """
    Traverse input_tree searching for nodes annotated as the starts of lineages.  For each lineage "root node", search
    for children of sibling nodes that have the same mutations and report those.
    """
    tree = bte.MATree(input_tree_file)
    counts = {}
    node_lineages = {}
    total_leaf_count = count_descendants_store_lineages(tree.root, counts, node_lineages)
    split_lineages = find_split_lineages_r(tree.root)
    print(f"Found {len(split_lineages)} split lineages in tree of {total_leaf_count} sequences:")
    # Sort by proportion of split_lineage number of descendants to its parent's, descending.
    split_lineages.sort(key=lambda sl: -counts[sl[2].id] / counts[sl[2].parent.id])
    report_splits(split_lineages, min_descendants, min_child_proportion, counts, node_lineages, exempt, prune_file)


def main():
    parser = argparse.ArgumentParser(description="Search for lineages with split-off parts placed under sibling nodes")
    parser.add_argument('-i', '--input-tree', required=True,
                        help="Input tree file in UShER protobuf format (.pb or .pb.gz)")
    parser.add_argument('-m', '--min-descendants', type=int, default=default_min_descendants,
                        help="Minimum number of leaf descendants in order to report a split-off branch " +
                        f"(default: {default_min_descendants})")
    parser.add_argument('-c', '--min-child-proportion', type=float, default=default_min_child_proportion,
                        help="Minimum proportion of split-off branch leaf descendants to its parent's in order to prune" +
                        f"(default: {default_min_child_proportion})")
    parser.add_argument('-e', '--exempt',
                        help="Comma-separated list of mutations to exempt from pruning unless tied in by one sequence")
    parser.add_argument('-p', '--prune-file',
                        help="Output file listing sequences to prune from the tree; re-optimizing afterwards should " +
                        "merge split branches.")
    args = parser.parse_args()
    exempt = args.exempt if args.exempt else ''
    exempt = set(exempt.split(','))
    find_split_lineages(args.input_tree, args.min_descendants, args.min_child_proportion, exempt, args.prune_file)


if __name__ == "__main__":
    main()
