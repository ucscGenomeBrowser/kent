"""
Mark low-quality tree branches (reversion nodes except those leading to annotated lineages, branches with relatively
many mutations and few descendants) to exclude from a lineage tree.  Then make a list of samples to retain in a
minimized subtree that will keep all annotated lineage nodes and descendant branches that make up a given proportion
of lineage descendants.  Output a list of samples for matUtils extract --samples (I would prune here, but have had
issues with BTE stability when removing nodes).
"""

import argparse
import bte
import copy
import csv
from dataclasses import dataclass, field
import gzip
import lzma
from mutation_set import MutationSet
import numpy as np
from numpy.typing import NDArray
import re
from typing import TextIO


def nonnegative_int(value: str) -> int:
    """argparse non-negative int checker"""
    iv = int(value)
    if iv < 0:
        raise argparse.ArgumentTypeError(f"Value must be non-negative (got {iv})")
    return iv


def proportion(value: str) -> float:
    """argparse proportion float checker"""
    fv = float(value)
    if fv < 0.0 or fv > 1.0:
        raise argparse.ArgumentTypeError(f"Value must be between 0.0 and 1.0 (got {fv})")
    return fv


def nonnegative_float(value: str) -> float:
    """argparse non-negative float checker"""
    fv = float(value)
    if fv < 0:
        raise argparse.ArgumentTypeError(f"Value must be non-negative (got {fv})")
    return fv


def parse_args():
    """Parse and check command line args using argparse"""
    default_lineage_index = 1           # Index of desired lineage annotation (default: SARS-CoV-2 Pango)
    default_min_keep_child = 0.1        # keep_largest_branches keeps and descends into child branches with >= this of lineage_leaf_count
    default_min_do_top_n_child = 0.3    # keep_largest_branches does top_n when descending into child branches with >= this of lineage_leaf_count
    default_top_n_branches = 5          # keep_largest_branches keeps branch if top N by same-lineage leaf count unless not doing top_n
    default_top_n_large_branches = 100  # keep_largest_branches keeps branch if top N and has leaf count >= top_n_large_factor * N
    default_top_n_large_factor = 3      # ''
    default_min_keep_reversion = 0.25   # node_is_excluded spares rev branches that make up >= this of lineage_leaf_count
    default_max_keep_discordant = 0.6   # node_is_excluded excludes non-lin branches with >= this discordant pangolin assignment vs placement
    default_max_mutation_density = 10   # node_is_excluded excludes non-lin branches with > this mutation density unless ignore_mut_density
    default_min_auto_add_child = 0.99   # add_n_closest descends into child if child has >= this of node same-lineage leaf count

    parser = argparse.ArgumentParser(description="Select samples to retain in a minimized lineage tree.")
    parser.add_argument('--debug', action='store_true',
                        help="Write diagnostic info to ./debug.tsv")

    io_group = parser.add_argument_group("Input/output file names")
    io_group.add_argument('-i', '--input-tree', required=True,
                          help="Input tree file in UShER protobuf format (.pb or .pb.gz)")
    io_group.add_argument('-m', '--input-metadata',
                          help="SARS-CoV-2/Pango specific: Input tab-separated metadata file with header that " +
                          "includes columns " +
                          "'strain' (values must match leaves of tree), " +
                          "'pangolin_lineage' (lineage assigned by pangolin tool), and " +
                          "'pango_lineage_usher' (lineage assigned by placement in tree)")
    io_group.add_argument('-o', '--output-samples', required=True,
                          help="Output file listing sequences to retain in the lineage tree.")

    annotation_group = parser.add_argument_group("Lineage annotation parameters")
    annotation_group.add_argument('--lineage-index', type=nonnegative_int, default=default_lineage_index,
                                  help="Index of desired annotation in each node's array of annotations "
                                  f"(default: {default_lineage_index} because SARS-CoV-2 trees have Nextstrain " +
                                  "clades at 0 and Pango at 1)")

    exclusion_group = parser.add_argument_group("Node exclusion/inclusion parameters")
    exclusion_group.add_argument('--max-keep-discordant', type=proportion, default=default_max_keep_discordant,
                                 help="Maximum proportion of leaves with discordant Pango lineage according to " +
                                 "metadata in order for a branch to be retained in tree " +
                                 f"(default: {default_max_keep_discordant}; meaningless without -m/--input-metadata)")
    exclusion_group.add_argument('--min-keep-reversion', type=proportion, default=default_min_keep_reversion,
                                 help="Minimum proportion of lineage leaf descendants (not including sub-lineages) " +
                                 "that a branch must represent in order to not be excluded if it has a reversion " +
                                 f"(default: {default_min_keep_reversion})")
    exclusion_group.add_argument('--max-mutation-density', type=nonnegative_float, default=default_max_mutation_density,
                                 help="Maximum mutation density to allow in a retained sub-lineage branch, " +
                                 "i.e. total mutations / number of descendants, calculated in a simpler way than " +
                                 "matUtils so use a significantly higher value " +
                                 f"(default: {default_max_mutation_density})")

    selection_group = parser.add_argument_group("Branch selection parameters")
    selection_group.add_argument('--min-auto-add-child', type=proportion, default=default_min_auto_add_child,
                                 help="Minimum proportion of descendants in order to automatically keep a child node " +
                                 f"(default: {default_min_auto_add_child})")
    selection_group.add_argument('--min-keep-child', type=proportion, default=default_min_keep_child,
                                 help="Minimum proportion of lineage descendants in order to keep a sub-lineage " +
                                 "branch " +
                                 f"(default: {default_min_keep_child})")
    selection_group.add_argument('--min-do-top-n-child', type=proportion, default=default_min_do_top_n_child,
                                 help="Minimum proportion of lineage descendants in order to apply top-n-branches " +
                                 "to a child of a lineage root node " +
                                 f"(default: {default_min_do_top_n_child})")
    selection_group.add_argument('--top-n-branches', type=nonnegative_int, default=default_top_n_branches,
                                 help="Retain the top n branches (by same-lineage leaf count) of a lineage root node " +
                                 f"(default: {default_top_n_branches})")
    selection_group.add_argument('--top-n-large-branches', type=nonnegative_int, default=default_top_n_large_branches,
                                 help="Like --top-n-branches with the additional constraint that the number of " +
                                 "leaves on a branch is at least --top-n-large-factor * rank " +
                                 f"(default: {default_top_n_large_branches})")
    selection_group.add_argument('--top-n-large-factor', type=nonnegative_int, default=default_top_n_large_factor,
                                 help="Number to multiply by rank of top-n-large-branches candidate to get minimum " +
                                 "number of leaves " +
                                 f"(default: {default_top_n_large_factor})")

    args = parser.parse_args()
    if not args.input_metadata:
        if args.max_keep_discordant != default_max_keep_discordant and \
           args.max_keep_discordant != 0.0:
            print(f"Warning: --max-keep-discordant={args.max_keep_discordant} was given but -m/--input-metadata was " +
                  "not provided, so --max-keep-discordant will have no effect.")


@dataclass
class Config:
    """Parameters for fine-tuning selection of lineage representatives"""
    lineage_index: int
    min_keep_child: float
    min_do_top_n_child: float
    top_n_branches: int
    top_n_large_branches: int
    top_n_large_factor: int
    min_keep_reversion: float
    max_keep_discordant: float
    max_mutation_density: float
    min_auto_add_child: float
    debug: bool


@dataclass
class NodeStats:
    """Per-node statistics to help flag quality issues and find lineages' largest child branches"""
    node_count: int
    node_to_ix: dict[str, int] = field(default_factory=dict)
    leaf_counts: NDArray[np.int32] = field(init=False)
    leaf_counts_same_lineage: NDArray[np.int32] = field(init=False)
    mut_counts: NDArray[np.int32] = field(init=False)
    discordant_counts: NDArray[np.int32] = field(init=False)
    is_lineage_ancestor: NDArray[bool] = field(init=False)
    has_lineage_reversion: NDArray[bool] = field(init=False)
    has_high_mut_density: NDArray[bool] = field(init=False)

    def __post_init__(self):
        self.leaf_counts = np.zeros(self.node_count, dtype=np.int32)
        self.leaf_counts_same_lineage = np.zeros(self.node_count, dtype=np.int32)
        self.mut_counts = np.zeros(self.node_count, dtype=np.int32)
        self.discordant_counts = np.zeros(self.node_count, dtype=np.int32)
        self.is_lineage_ancestor = np.zeros(self.node_count, dtype=bool)
        self.has_lineage_reversion = np.zeros(self.node_count, dtype=bool)
        self.has_high_mut_density = np.zeros(self.node_count, dtype=bool)


@dataclass
class ParentState:
    """Parental node lineage and mutation state that we maintain while traversing the tree to gather stats"""
    lineage: str
    lineage_muts: MutationSet
    all_muts: MutationSet
    has_lineage_rev: bool


@dataclass
class SearchState:
    """State that we keep and parameters we tweak while traversing the tree to select lineage representatives"""
    ignore_mut_density: bool
    ignore_rev: bool
    do_top_n: bool
    lineage_leaf_count: int
    lineage: str
    debug_f: TextIO


def openMaybeDecompress(filename: str) -> TextIO:
    if filename.endswith('.gz'):
        return gzip.open(filename, 'rt')
    elif filename.endswith('.xz'):
        return lzma.open(filename, 'rt')
    else:
        return open(filename, 'r')


def discordant_leaves_from_metadata(input_metadata_file: str) -> set[str]:
    """
    input_metadata_file must be a TSV file with header (possibly compressed) that corresponds to tree. Required columns:
    * strain: must match names of leaves of tree
    * pangolin_lineage: Pango lineage assigned to each leaf sequence by pangolin
    * pango_lineage_usher: Pango lineage assigned by placement in annotated tree; may contain extra labels and new
                           lineages not yet assignable by pangolin.
    Return the set of names of leaves that have a clearly discordant Pango lineage assignment by those two methods.
    Not all mismatches are necessarily discordant.  For example, we expect changes when there's a new lineage annotated
    on the tree but not (yet) known to pangolin, or when a lineage has been retracted but is still called by pangolin,
    or when an old lineage is designation-only (not annotated on the tree because it's non-monophyletic).
    We ignore rows with missing values and/or pangolin 'Unassigned'.
    """
    discordant_leaves = set()
    # These are some pre-usher-era Pango lineages that are not monophyletic on the UShER tree so they're not annotated,
    # but they're still designated so pangolin assigns these to the designated representative sequences.
    designated_not_annotated = set(["B.1.1.184", "B.1.1.230", "B.1.1.31", "B.1.1.319", "B.1.1.334", "B.1.1.335",
                                    "B.1.1.340", "B.1.1.370", "B.1.1.409", "B.1.1.410", "B.1.1.411", "B.1.1.419",
                                    "B.1.1.52", "B.1.119", "B.1.149", "B.1.169", "B.1.214.2", "B.1.247", "B.1.258.21",
                                    "B.1.258.24", "B.1.260", "B.1.285", "B.1.351.1", "B.1.371", "B.1.406", "B.1.434",
                                    "B.1.446", "B.1.527", "B.1.537", "B.1.538", "B.1.566", "B.49"])
    lineage_underscore = re.compile(r'^([A-Z]+[.0-9]*)_.*')
    maybe_discordant = dict()
    ignored_count = 0
    concordant_count = 0
    with openMaybeDecompress(input_metadata_file) as mf:
        reader = csv.DictReader(mf, delimiter='\t')
        observed_pangolin_lineages = set()
        observed_pango_usher_lineages = set()
        for row in reader:
            name = row["strain"]
            pangolin_lineage = row["pangolin_lineage"]
            pango_lineage_usher = row["pango_lineage_usher"]
            if pangolin_lineage == "Unassigned" or not pangolin_lineage or not pango_lineage_usher:
                ignored_count += 1
                continue
            # If pango_lineage_usher is a non-Pango symbolic label, ignore it.
            if not pango_lineage_usher[0].isupper():
                ignored_count += 1
                continue
            # If pango_lineage_usher is an extra lineage label (Pango lineage followed by underscore followed by
            # description) then strip the extra part to get just the lineage for comparison.
            underscore_match = lineage_underscore.search(pango_lineage_usher)
            if underscore_match:
                pango_lineage_usher = underscore_match.groups()[0]
            observed_pangolin_lineages.add(pangolin_lineage)
            observed_pango_usher_lineages.add(pango_lineage_usher)
            if pangolin_lineage == pango_lineage_usher:
                concordant_count += 1
            else:
                maybe_discordant[name] = (pangolin_lineage, pango_lineage_usher)
    new_lineages = observed_pango_usher_lineages - observed_pangolin_lineages
    print(f"New lineages in tree not assigned by pangolin:\n{new_lineages}")
    removed_lineages = observed_pangolin_lineages - observed_pango_usher_lineages - designated_not_annotated
    if len(removed_lineages) > 0:
        print(f"*** Lineages assigned by pangolin not present in tree (removed/retracted?):\n{removed_lineages}")
    changed_count = 0
    discordant_count = 0
    for name, (pangolin_lineage, pango_lineage_usher) in maybe_discordant.items():
        if pango_lineage_usher in new_lineages or pangolin_lineage in removed_lineages or \
           pangolin_lineage in designated_not_annotated:
            changed_count += 1
        else:
            discordant_count += 1
            discordant_leaves.add(name)
    print(f"{concordant_count} matching Pango assignments, {ignored_count} ignored assignments, " +
          f"{changed_count} expected-changed assignments, {discordant_count} mismatching Pango assignments")
    return discordant_leaves


def count_nodes(node: bte.MATNode) -> int:
    """
    Traverse node and its descendants to count the total number of nodes including internal nodes and leaves.
    """
    count = 1
    for child in node.children:
        count += count_nodes(child)
    return count


def build_node_stats(node: bte.MATNode, node_ix: int, discordant_leaves: set[str], config: Config, parent: ParentState,
                     node_stats: NodeStats
                     ) -> (int, int, int, int, bool, int):
    """
    Traverse from node to count each node's number of leaves, total number of mutations including its own and its
    descendants', whether the node is a lineage root or has a descendant that is a lineage root, and whether the node
    has reversions of any of its lineage's mutations.
    """
    node_stats.node_to_ix[node.id] = node_ix
    node_count = 1
    leaf_count = 0 if node.children else 1
    leaf_count_same_lin = leaf_count
    discordant_count = 1 if node.id in discordant_leaves else 0
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
        (c_node_count, c_leaf_count, c_leaf_count_same_lin, c_mut_count, c_is_lin, c_discordant_count) = (
            build_node_stats(child, node_ix + node_count, discordant_leaves, config, my_state, node_stats))
        node_count += c_node_count
        leaf_count += c_leaf_count
        leaf_count_same_lin += c_leaf_count_same_lin
        children_mut_count += c_mut_count
        is_lin |= c_is_lin
        discordant_count += c_discordant_count
    node_stats.leaf_counts[node_ix] = leaf_count
    node_stats.leaf_counts_same_lineage[node_ix] = leaf_count_same_lin
    node_stats.mut_counts[node_ix] = own_mut_count + children_mut_count
    node_stats.discordant_counts[node_ix] = discordant_count
    node_stats.is_lineage_ancestor[node_ix] = is_lin
    node_stats.has_lineage_reversion[node_ix] = has_rev
    # matUtils' calculation of mutation density is much more nuanced than advertised.
    # This is simpler.  It doesn't exempt small lineages (<150 leaves) or tight clusters, nor prune leaves with a lot
    # of mutations.  Another tree traversal would be required in order to calculate finer-grained mutation density
    # like matUtils, so to save time, I'm just going with a simplified version and using a higher default threshold.
    # This here is the tip of the matUtils iceberg: node's mutations are downweighted to avoid penalizing a long branch
    # to a decent cluster:
    mut_density = ((own_mut_count / leaf_count) + children_mut_count) / leaf_count
    node_stats.has_high_mut_density[node_ix] = (mut_density > config.max_mutation_density)
    ret_leaf_count_same_lin = 0 if is_lin_start else leaf_count_same_lin
    ret_mut_count = own_mut_count + children_mut_count
    return (node_count, leaf_count, ret_leaf_count_same_lin, ret_mut_count, is_lin, discordant_count)


def node_is_excluded(node_id: str, config: Config, node_stats: NodeStats, search_state: SearchState):
    """
    Return True if node has sufficiently bad quality according to node_stats and search_state.
    If ignore_mut_density, then don't gate with has_high_mut_density but still check for has_lineage_reversion,
    unless ignore_rev.
    """
    node_ix = node_stats.node_to_ix[node_id]
    is_lin = node_stats.is_lineage_ancestor[node_ix]
    if search_state.lineage_leaf_count:
        lin_proportion = node_stats.leaf_counts_same_lineage[node_ix] / search_state.lineage_leaf_count
    else:
        print(search_state)
        lin_proportion = 0
    is_major_lin_branch = lin_proportion >= config.min_keep_reversion
    has_rev = node_stats.has_lineage_reversion[node_ix]
    has_high_mut_density = node_stats.has_high_mut_density[node_ix]
    discordance = node_stats.discordant_counts[node_ix] / node_stats.leaf_counts[node_ix]
    has_high_discordance = discordance > config.max_keep_discordant
    if config.debug and not is_lin and has_high_discordance:
        print(f"{search_state.lineage}\t{node_id} excluded with discordance " +
              f"{node_stats.discordant_counts[node_ix]} / {node_stats.leaf_counts[node_ix]} = {discordance}\t",
              file=search_state.debug_f)
    return (not is_lin and
            (has_high_discordance or
             (has_high_mut_density and not search_state.ignore_mut_density) or
             (has_rev and not is_major_lin_branch and not search_state.ignore_rev)))


def add_n_closest(node: bte.MATNode, n: int, config: Config, node_stats: NodeStats, search_state: SearchState,
                  lineage_reps: set[str]) -> int:
    """
    Try to find n leaf children with no mutations of their own, and add them to lineage_reps.
    If there aren't n, then try the most similar leaves, then try the most similar non-excluded child branches.
    Return the number of leaf descendants added to lineage_reps.
    """
    found_count = 0
    node_ix = node_stats.node_to_ix[node.id]
    node_leaf_count_same_lin = node_stats.leaf_counts_same_lineage[node_ix]
    for child in node.children:
        if not child.children and len(child.mutations) == 0 and child.id not in lineage_reps:
            lineage_reps.add(child.id)
            found_count += 1
            if config.debug:
                print(f"{search_state.lineage}\t{child.id}\t0", file=search_state.debug_f)
            if found_count == n:
                break
        elif child.children and not child.annotations[config.lineage_index]:
            # If child makes up 99% of same-lineage descendants then add child too
            if not node_is_excluded(child.id, config, node_stats, search_state):
                c_node_ix = node_stats.node_to_ix[child.id]
                c_leaf_count_same_lin = node_stats.leaf_counts_same_lineage[c_node_ix]
                if c_leaf_count_same_lin / node_leaf_count_same_lin >= config.min_auto_add_child:
                    add_n_closest(child, n, config, node_stats, search_state, lineage_reps)
    if found_count < n:
        sorted_children = sorted(node.children, key=lambda c: len(c.mutations))
        for child in sorted_children:
            if not child.children and child.id not in lineage_reps:
                lineage_reps.add(child.id)
                found_count += 1
                if config.debug:
                    print(f"{search_state.lineage}\t{child.id}\t{len(child.mutations)}", file=search_state.debug_f)
                if found_count == n:
                    break
        if found_count < n:
            for child in sorted_children:
                if child.children and not child.annotations[config.lineage_index]:
                    if not node_is_excluded(child.id, config, node_stats, search_state):
                        found_count += add_n_closest(child, n - found_count, config, node_stats, search_state,
                                                     lineage_reps)
                        if found_count >= n:
                            break
    return found_count


def keep_largest_branches(node: bte.MATNode, config: Config, node_stats: NodeStats, search_state: SearchState,
                          lineage_reps: set[str]) -> int:
    """
    For each descendant branch that is not itself a lineage branch and not excluded and has at least
    min_keep_child of the leaves in lin_leaf_count (or is in the top n branches by same-lineage leaf count),
    add a couple samples to lineage_reps so that branch is retained in the lineage tree.
    """
    found_count = 0
    candidate_branches = sorted([child for child in node.children
                                 if child.children and not child.annotations[config.lineage_index]],
                                key=lambda c: -node_stats.leaf_counts_same_lineage[node_stats.node_to_ix[c.id]])
    kept_branch_count = 0
    # Don't pass do_top_n down for child branches, it's only for the lineage top level (unless the child branch has
    # most of the parent's leaves).
    recurse_search_state = copy.copy(search_state)
    for child in candidate_branches:
        if not node_is_excluded(child.id, config, node_stats, search_state):
            c_node_ix = node_stats.node_to_ix[child.id]
            child_proportion = node_stats.leaf_counts_same_lineage[c_node_ix] / search_state.lineage_leaf_count
            child_is_large = (child_proportion >= config.min_keep_child)
            child_do_top_n = (child_proportion >= config.min_do_top_n_child)
            child_leaf_count = node_stats.leaf_counts[c_node_ix]
            child_is_top_n = (search_state.do_top_n and
                              (kept_branch_count < config.top_n_branches or
                               (kept_branch_count < config.top_n_large_branches and
                                child_leaf_count >= (config.top_n_large_factor * (kept_branch_count + 1)))))
            if child_is_large or child_is_top_n:
                kept_branch_count += 1
                found_count += add_n_closest(child, 2, config, node_stats, search_state, lineage_reps)
                # Normally we don't want to apply top-n for branches below the lineage top level.
                # But sometimes most of the lineage samples are on one child branch.  In that case, that child branch
                # has most of the top n branches
                recurse_search_state.do_top_n = child_do_top_n
                if child_is_large:
                    found_count += keep_largest_branches(child, config, node_stats, recurse_search_state, lineage_reps)
    return found_count


def select_lineage_representatives(node: bte.MATNode, config: Config, node_stats: NodeStats, lineage_reps: set[str],
                                   parent_lineage_leaf_count: int, parent_lineage: str, debug_f: TextIO):
    """
    Traverse from node to find lineage roots, and for each lineage, select representative samples as follows:
    * exclude children in exclude_node
    * try to find two leaf children with no mutations relative to the lineage; otherwise find the two samples
      that are most similar to the lineage root and add them to the set
    * for each non-lineage-root child whose leaf count is at least min_keep_child of the lineage root's
      leaf count or in the top n branches by leaf count, find its two most similar samples and add them to the set
    * recurse only to children with is_lineage_ancestor
    """
    search_state = SearchState(ignore_mut_density=False, ignore_rev=False, do_top_n=True,
                               lineage_leaf_count=parent_lineage_leaf_count, lineage=parent_lineage, debug_f=debug_f)
    node_ix = node_stats.node_to_ix[node.id]
    if node.annotations[config.lineage_index]:
        # First, try to find representatives that aren't excluded with normal search_state settings.
        search_state.lineage = node.annotations[config.lineage_index]
        search_state.lineage_leaf_count = node_stats.leaf_counts_same_lineage[node_ix]
        found_count = add_n_closest(node, 2, config, node_stats, search_state, lineage_reps)
        found_count += keep_largest_branches(node, config, node_stats, search_state, lineage_reps)
        if found_count < 2 or found_count < 6 and search_state.lineage_leaf_count > 3:
            # Try harder: ignore mutation density but still try to avoid reversion branches.
            search_state.ignore_mut_density = True
            found_count += add_n_closest(node, 2, config, node_stats, search_state, lineage_reps)
            found_count += keep_largest_branches(node, config, node_stats, search_state, lineage_reps)
            if found_count < 2 or found_count < 6 and search_state.lineage_leaf_count > 3:
                # Try harder still: also ignore reversion flags.
                search_state.ignore_rev = True
                found_count += add_n_closest(node, 2, config, node_stats, search_state, lineage_reps)
                found_count += keep_largest_branches(node, config, node_stats, search_state, lineage_reps)
        if config.debug and found_count < 6 and found_count < search_state.lineage_leaf_count:
            print(f"{search_state.lineage}\tFEW FOUND\t{found_count} < {search_state.lineage_leaf_count}",
                  file=search_state.debug_f)
    elif node_stats.is_lineage_ancestor[node_ix]:
        # Try to save a leaf so this node doesn't get collapsed in pruning.  We need intermediate nodes to stay intact
        # to avoid long branch attraction, e.g. of sequences with lots of revs to a long rev branch to recombinant.
        add_n_closest(node, 1, config, node_stats, search_state, lineage_reps)
    for child in node.children:
        if child.children:
            c_node_ix = node_stats.node_to_ix[child.id]
            if node_stats.is_lineage_ancestor[c_node_ix]:
                select_lineage_representatives(child, config, node_stats, lineage_reps, search_state.lineage_leaf_count,
                                               search_state.lineage, debug_f)


def write_set_to_file(input_set: set[str], output_file: str):
    with open(output_file, "w") as outf:
        for thing in input_set:
            print(thing, file=outf)


def make_lineage_tree_sample_set(tree: bte.MATree, discordant_leaves: set[str], config: Config) -> set[str]:
    """
    Collect stats on every node in tree so we can identify "good" and "bad" branches, then use those stats to
    select sequences on good representative nodes for each annotated lineage.
    """
    # Count total number of nodes (internal and leaves) so we can initialize NodeStats arrays
    node_count = count_nodes(tree.root)
    print(f"{node_count} nodes")
    node_stats = NodeStats(node_count=node_count)
    # Count each node's number descendants, number of mutations, whether it (or a descendant) has an annotated
    # lineage, whether it has a reversion relative to its most recent annotated-lineage ancestor.
    no_mutations = MutationSet([])
    parent_state = ParentState(lineage='', lineage_muts=no_mutations, all_muts=no_mutations, has_lineage_rev=False)
    build_node_stats(tree.root, node_ix=0, discordant_leaves=discordant_leaves, config=config, parent=parent_state,
                     node_stats=node_stats)
    print(f"{node_stats.leaf_counts[0]} leaves, {node_stats.leaf_counts_same_lineage[0]} leaves same lin, " +
          f"{node_stats.mut_counts[0]} muts, " +
          f"{(node_stats.is_lineage_ancestor).sum()} lineage-ancestral nodes, " +
          f"{(node_stats.has_lineage_reversion).sum()} nodes on lin-rev branches")
    # Select lineage representative samples so that we can extract a minimized lineage annotation tree
    lineage_reps: set[str] = set()
    with open("debug.tsv" if config.debug else "/dev/null", "w") as debug_f:
        select_lineage_representatives(tree.root, config=config, node_stats=node_stats, lineage_reps=lineage_reps,
                                       parent_lineage_leaf_count=0, parent_lineage='', debug_f=debug_f)
    return lineage_reps


def make_lineage_tree_sample_list(input_tree_file: str, input_metadata_file: str, config: Config,
                                  output_samples_file: str):
    """Read input tree and optional metadata, select lineage representatives, write them to output_samples_file."""
    print(config)
    if input_metadata_file:
        print(f"Reading {input_metadata_file}...")
        discordant_leaves = discordant_leaves_from_metadata(input_metadata_file)
    else:
        discordant_leaves = set()
    print(f"Reading {input_tree_file}...")
    tree = bte.MATree(input_tree_file)
    lineage_reps = make_lineage_tree_sample_set(tree, discordant_leaves, config)
    print(f"Selected {len(lineage_reps)} samples, writing to {output_samples_file}.")
    # Write output file of lineage representative samples
    write_set_to_file(lineage_reps, output_samples_file)


def main():
    args = parse_args()
    config = Config(min_keep_child=args.min_keep_child,
                    min_do_top_n_child=args.min_do_top_n_child,
                    min_keep_reversion=args.min_keep_reversion,
                    max_keep_discordant=args.max_keep_discordant,
                    lineage_index=args.lineage_index,
                    max_mutation_density=args.max_mutation_density,
                    top_n_branches=args.top_n_branches,
                    top_n_large_branches=args.top_n_large_branches,
                    top_n_large_factor=args.top_n_large_factor,
                    min_auto_add_child=args.min_auto_add_child,
                    debug=bool(args.debug))
    make_lineage_tree_sample_list(args.input_tree, args.input_metadata, config, args.output_samples)


if __name__ == "__main__":
    main()
