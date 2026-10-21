"""
Mark low-quality tree branches (reversion nodes except those leading to annotated lineages, branches with relatively
many mutations and few descendants) to exclude from a lineage tree.  Then make a list of samples to retain in a
minimized subtree that will keep all annotated lineage nodes and descendant branches that make up a given proportion
of lineage descendants.  Output a list of samples for matUtils extract --samples (I would prune here, but have had
issues with BTE stability when removing nodes).
"""

import argparse
import bte
from dataclasses import dataclass, field
from mutation_set import MutationSet
import numpy as np
from numpy.typing import NDArray

default_min_descendant_proportion = 0.1
default_lineage_index = 1
default_max_mutation_density = 10
default_top_n_branches = 5

debug = True

@dataclass
class NodeStats:
    """Per-node statistics to help flag quality issues and find lineages' largest child branches"""
    node_count: int
    node_to_ix: dict[str, int] = field(default_factory=dict)
    leaf_counts: NDArray[np.int32] = field(init=False)
    leaf_counts_same_lineage: NDArray[np.int32] = field(init=False)
    mut_counts: NDArray[np.int32] = field(init=False)
    is_lineage_ancestor: NDArray[bool] = field(init=False)
    has_lineage_reversion: NDArray[bool] = field(init=False)
    exclude_node: NDArray[bool] = field(init=False)

    def __post_init__(self):
        self.leaf_counts = np.zeros(self.node_count, dtype=np.int32)
        self.leaf_counts_same_lineage = np.zeros(self.node_count, dtype=np.int32)
        self.mut_counts = np.zeros(self.node_count, dtype=np.int32)
        self.is_lineage_ancestor = np.zeros(self.node_count, dtype=bool)
        self.has_lineage_reversion = np.zeros(self.node_count, dtype=bool)
        self.exclude_node = np.zeros(self.node_count, dtype=bool)


@dataclass
class ParentState:
    """Parental node lineage and mutation state that we maintain while traversing the tree"""
    lineage: str
    lineage_muts: MutationSet
    all_muts: MutationSet
    has_lineage_rev: bool


@dataclass
class Config:
    """Parameters for fine-tuning selection of lineage representatives"""
    min_descendant_proportion: float
    lineage_index: int
    max_mutation_density: float
    top_n_branches: int


def count_nodes(node: bte.MATNode) -> int:
    """
    Traverse node and its descendants to count the total number of nodes including internal nodes and leaves.
    """
    count = 1
    for child in node.children:
        count += count_nodes(child)
    return count


def build_node_stats(node: bte.MATNode, node_ix: int, config: Config, parent: ParentState, node_stats: NodeStats
                     ) -> (int, int, int, int, bool):
    """
    Traverse from node to count each node's number of leaves, total number of mutations including its own and its
    descendants', whether the node is a lineage root or has a descendant that is a lineage root, and whether the node
    has reversions of any of its lineage's mutations.
    """
    node_stats.node_to_ix[node.id] = node_ix
    node_count = 1
    leaf_count = 0 if node.children else 1
    leaf_count_same_lin = leaf_count
    own_mut_count = len(node.mutations)
    is_lin_start = bool(node.annotations[config.lineage_index])
    is_lin = is_lin_start
    node_muts = MutationSet(node.mutations)
    my_muts = parent.all_muts.with_mutations(node_muts)
    lineage = node.annotations[config.lineage_index] if is_lin else parent.lineage
    has_rev = False if is_lin else (parent.has_lineage_rev or
                                    parent.lineage_muts.has_reversion(node_muts))
    lin_muts = my_muts if is_lin else parent.lineage_muts
    my_state = ParentState(lineage=lineage, lineage_muts=lin_muts, all_muts=my_muts, has_lineage_rev=has_rev)
    children_mut_count = 0
    for child in node.children:
        (c_node_count, c_leaf_count, c_leaf_count_same_lin, c_mut_count, c_is_lin) = (
            build_node_stats(child, node_ix + node_count, config, my_state, node_stats))
        node_count += c_node_count
        leaf_count += c_leaf_count
        leaf_count_same_lin += c_leaf_count_same_lin
        children_mut_count += c_mut_count
        is_lin |= c_is_lin
    node_stats.leaf_counts[node_ix] = leaf_count
    node_stats.leaf_counts_same_lineage[node_ix] = leaf_count_same_lin
    node_stats.mut_counts[node_ix] = own_mut_count + children_mut_count
    node_stats.is_lineage_ancestor[node_ix] = is_lin
    node_stats.has_lineage_reversion[node_ix] = has_rev
    # matUtils' calculation of mutation density is much more nuanced than advertised.
    # This is simpler.  It doesn't exempt small lineages (<150 leaves) or tight clusters, nor prune leaves with a lot
    # of mutations.  Another tree traversal would be required in order to calculate finer-grained mutation density
    # like matUtils, so to save time, I'm just going with a simplified version and using a higher default threshold.
    # This here is the tip of the matUtils iceberg: node's mutations are downweighted to avoid penalizing a long branch
    # to a decent cluster:
    mut_density = ((own_mut_count / leaf_count) + children_mut_count) / leaf_count
    node_stats.exclude_node[node_ix] = ((has_rev or mut_density > config.max_mutation_density) and not is_lin)
    ret_leaf_count_same_lin = 0 if is_lin_start else leaf_count_same_lin
    return (node_count, leaf_count, ret_leaf_count_same_lin, own_mut_count + children_mut_count, is_lin)


def add_n_closest(node: bte.MATNode, n: int, lin_ix: int, node_stats: NodeStats, ignore_excluded: bool,
                  ignore_rev: bool, lineage_reps: set[str], lineage: str, debug_f) -> int:
    """
    Try to find n leaf children with no mutations of their own, and add them to lineage_reps.
    If there aren't n, then try the most similar leaves, then try the most similar non-excluded child branches.
    If ignore_excluded, then don't gate with exclude_node but still check for has_lineage_reversion, unless ignore_rev.
    """
    found_count = 0
    for child in node.children:
        if not child.children and len(child.mutations) == 0 and child.id not in lineage_reps:
            lineage_reps.add(child.id)
            found_count += 1
            if debug:
                print(f"{lineage}\t{child.id}\t0", file=debug_f)
            if found_count == n:
                break
    if found_count < n:
        sorted_children = sorted(node.children, key=lambda c: len(c.mutations))
        for child in sorted_children:
            if not child.children and child.id not in lineage_reps:
                lineage_reps.add(child.id)
                found_count += 1
                if debug:
                    print(f"{lineage}\t{child.id}\t{len(child.mutations)}", file=debug_f)
                if found_count == n:
                    break
        if found_count < n:
            for child in sorted_children:
                if child.children and not child.annotations[lin_ix]:
                    c_node_ix = node_stats.node_to_ix[child.id]
                    is_excluded = node_stats.exclude_node[c_node_ix]
                    has_rev = node_stats.has_lineage_reversion[c_node_ix]
                    if not is_excluded or (ignore_excluded and (not has_rev or ignore_rev)):
                        found_count += add_n_closest(child, n - found_count, lin_ix, node_stats,
                                                     ignore_excluded, ignore_rev, lineage_reps, lineage, debug_f)
                        if found_count >= n:
                            break
    return found_count


def keep_largest_branches(node: bte.MATNode, config: Config, lin_leaf_count: int, node_stats: NodeStats,
                          ignore_excluded: bool, ignore_rev: bool, do_top_n: bool, lineage_reps: set[str], lineage: str, debug_f) -> int:
    """
    For each descendant branch that is not itself a lineage branch and not excluded and has at least
    min_descendant_proportion of the leaves in lin_leaf_count (or is in the top n branches by same-lineage leaf count),
    add a couple samples to lineage_reps so that branch is retained in the lineage tree.
    """
    found_count = 0
    candidate_branches = sorted([child for child in node.children
                                 if child.children and not child.annotations[config.lineage_index]],
                                key=lambda c: -node_stats.leaf_counts_same_lineage[node_stats.node_to_ix[c.id]])
    kept_branch_count = 0
    for child in candidate_branches:
        c_node_ix = node_stats.node_to_ix[child.id]
        is_excluded = node_stats.exclude_node[c_node_ix]
        has_rev = node_stats.has_lineage_reversion[c_node_ix]
        if not is_excluded or (ignore_excluded and (not has_rev or ignore_rev)):
            child_proportion = node_stats.leaf_counts_same_lineage[c_node_ix] / lin_leaf_count
            if (child_proportion >= config.min_descendant_proportion or
                (do_top_n and kept_branch_count < config.top_n_branches)):
                found_count += add_n_closest(child, 2, config.lineage_index, node_stats, ignore_excluded, ignore_rev,
                                             lineage_reps, lineage, debug_f)
                # Don't pass do_top_n down, it's only for the lineage top level.
                found_count += keep_largest_branches(child, config, lin_leaf_count, node_stats, ignore_excluded,
                                                     ignore_rev, False, lineage_reps, lineage, debug_f)
                kept_branch_count += 1
    return found_count


def select_lineage_representatives(node: bte.MATNode, config: Config, node_stats: NodeStats,
                                   lineage_reps: set[str], parent_lineage: str, debug_f
                                   ) -> int:
    """
    Traverse from node to find lineage roots, and for each lineage, select representative samples as follows:
    * exclude children in exclude_node
    * try to find two leaf children with no mutations relative to the lineage; otherwise find the two samples
      that are most similar to the lineage root and add them to the set
    * for each non-lineage-root child whose leaf count is at least min_descendant_proportion of the lineage root's
      leaf count or in the top n branches by leaf count, find its two most similar samples and add them to the set
    * recurse only to children with is_lineage_ancestor
    """
    lineage = parent_lineage
    ignore_excluded = False
    ignore_rev = False
    do_top_n = True
    if node.annotations[config.lineage_index]:
        # First, try to find representatives that aren't excluded.
        lineage = node.annotations[config.lineage_index]
        found_count = add_n_closest(node, 2, config.lineage_index, node_stats, ignore_excluded, ignore_rev,
                                    lineage_reps, lineage, debug_f)
        node_ix = node_stats.node_to_ix[node.id]
        lin_leaf_count = node_stats.leaf_counts_same_lineage[node_ix]
        found_count += keep_largest_branches(node, config, lin_leaf_count, node_stats, ignore_excluded,
                                             ignore_rev, do_top_n, lineage_reps, lineage, debug_f)
        if found_count < 2 or found_count < 6 and lin_leaf_count > 3:
            # Try harder: ignore excluded_node but still try to avoid reversion branches.
            ignore_excluded = True
            found_count += add_n_closest(node, 2, config.lineage_index, node_stats, ignore_excluded, ignore_rev,
                                         lineage_reps, lineage, debug_f)
            found_count += keep_largest_branches(node, config, lin_leaf_count, node_stats, ignore_excluded,
                                                 ignore_rev, do_top_n, lineage_reps, lineage, debug_f)
            if found_count < 2 or found_count < 6 and lin_leaf_count > 3:
                # Try harder still: also ignore reversion flags.
                ignore_rev = True
                found_count += add_n_closest(node, 2, config.lineage_index, node_stats, ignore_excluded, ignore_rev,
                                             lineage_reps, lineage, debug_f)
                found_count += keep_largest_branches(node, config, lin_leaf_count, node_stats,
                                                     ignore_excluded, ignore_rev, do_top_n, lineage_reps, lineage, debug_f)
        if debug and found_count < 6 and found_count < lin_leaf_count:
            print(f"{lineage}\tFEW FOUND\t{found_count} < {lin_leaf_count}", file=debug_f)
    elif node_stats.is_lineage_ancestor[node_stats.node_to_ix[node.id]]:
        # Try to save a leaf so this node doesn't get collapsed in pruning.  We need intermediate nodes to stay intact
        # to avoid long branch attraction, e.g. of sequences with lots of revs to a long rev branch to recombinant.
        add_n_closest(node, 1, config.lineage_index, node_stats, ignore_excluded, ignore_rev, lineage_reps, lineage, debug_f)
    for child in node.children:
        if child.children:
            c_node_ix = node_stats.node_to_ix[child.id]
            if node_stats.is_lineage_ancestor[c_node_ix]:
                select_lineage_representatives(child, config, node_stats, lineage_reps, lineage, debug_f)


def write_set_to_file(input_set: set[str], output_file: str):
    with open(output_file, "w") as outf:
        for thing in input_set:
            print(thing, file=outf)


def make_lineage_tree_sample_list(input_tree_file: str, config: Config, output_samples_file: str):
    tree = bte.MATree(input_tree_file)

    # Count total number of nodes (internal and leaves) so we can initialize NodeStats arrays
    node_count = count_nodes(tree.root)
    print(f"{node_count} nodes, desc_prop={config.min_descendant_proportion}, " +
          f"top_n_branches={config.top_n_branches}, mut_d={config.max_mutation_density}")
    node_stats = NodeStats(node_count=node_count)
    # Count each node's number descendants, number of mutations, whether it (or a descendant) has an annotated
    # lineage, whether it has a reversion relative to its most recent annotated-lineage ancestor.
    no_mutations = MutationSet([])
    parent_state = ParentState(lineage='', lineage_muts=no_mutations, all_muts=no_mutations, has_lineage_rev=False)
    build_node_stats(tree.root, node_ix=0, config=config, parent=parent_state, node_stats=node_stats)
    print(f"{node_stats.leaf_counts[0]} leaves, {node_stats.leaf_counts_same_lineage[0]} leaves same lin, {node_stats.mut_counts[0]} muts, " +
          f"{(node_stats.is_lineage_ancestor).sum()} lineage-ancestral nodes, " +
          f"{(node_stats.has_lineage_reversion).sum()} nodes on lin-rev branches")
    # Select lineage representative samples so that we can extract a minimized lineage annotation tree
    lineage_reps: set[str] = set()
    with open("debug.tsv" if debug else "/dev/null", "w") as debug_f:
        select_lineage_representatives(tree.root, config=config, node_stats=node_stats, lineage_reps=lineage_reps,
                                       parent_lineage='', debug_f=debug_f)
    print(f"Selected {len(lineage_reps)} samples, writing to {output_samples_file}.")
    # Write output file of lineage representative samples
    write_set_to_file(lineage_reps, output_samples_file)


def main():
    parser = argparse.ArgumentParser(description="Select samples to retain in a minimized lineage tree.")
    parser.add_argument('-i', '--input-tree', required=True,
                        help="Input tree file in UShER protobuf format (.pb or .pb.gz)")
    parser.add_argument('-d', '--min-descendant-proportion', type=float, default=default_min_descendant_proportion,
                        help="Minimum proportion of lineage descendants in order to keep a sub-lineage branch " +
                        f"(default: {default_min_descendant_proportion})")
    parser.add_argument('-l', '--lineage-index', type=int, default=default_lineage_index,
                        help="Annotation array index "
                        "(defaults to 1 because SARS-CoV-2 trees have Nextstrain clades at 0 and Pango at 1)")
    parser.add_argument('-m', '--max-mutation-density', type=float, default=default_max_mutation_density,
                        help="Maximum mutation density to allow in a retained sub-lineage branch ," +
                        "i.e. total mutations / number of descendants, calculated in a simpler way than " +
                        "matUtils so use a significantly higher value" +
                        f"(default: {default_max_mutation_density})")
    parser.add_argument('-t', '--top-n-branches', type=int, default=default_top_n_branches,
                        help="Retain the top n branches (by same-lineage leaf count) of a lineage root node " +
                        f"(default: {default_top_n_branches})")
    parser.add_argument('-o', '--output-samples', required=True,
                        help="Output file listing sequences to retain in the lineage tree.")
    args = parser.parse_args()
    config = Config(min_descendant_proportion=args.min_descendant_proportion,
                    lineage_index=args.lineage_index,
                    max_mutation_density=args.max_mutation_density,
                    top_n_branches=args.top_n_branches)
    make_lineage_tree_sample_list(args.input_tree, config, args.output_samples)


if __name__ == "__main__":
    main()
