"""Sorted sets of nucleotide substitutions such as 'A123T'.

Standard library only. Requires Python 3.10+ (bisect's `key` argument).
Written by Claude Sonnet 5.5
"""

import re
from bisect import bisect_left
from typing import Iterable, Iterator, NamedTuple, Optional, Union

_MUT_RE = re.compile(r'^([A-Za-z])(\d+)([A-Za-z])$')


class Mutation(NamedTuple):
    """One substitution: parental base, 1-based position, mutated base."""
    par: str
    pos: int
    mut: str

    @classmethod
    def parse(cls, s: str) -> 'Mutation':
        m = _MUT_RE.match(s.strip())
        if m is None:
            raise ValueError(f'Bad mutation string: {s!r}')
        par, pos, mut = m.group(1).upper(), int(m.group(2)), m.group(3).upper()
        if par == mut:
            raise ValueError(f'Parental and mutated base are identical: {s!r}')
        return cls(par, pos, mut)

    def __str__(self) -> str:
        return f'{self.par}{self.pos}{self.mut}'


def _pos_of(m: Mutation) -> int:
    return m.pos


class MutationSet:
    """Mutations kept sorted by position, at most one per position."""

    def __init__(self, mutations: Iterable[Union[str, Mutation]] = ()):
        self._muts: list[Mutation] = []
        for m in mutations:
            self.add(m)

    # -- binary searches ---------------------------------------------------
    def _index(self, pos: int) -> int:
        """Index of the first Mutation with position >= pos."""
        return bisect_left(self._muts, pos, key=_pos_of)

    def find(self, pos: int) -> Optional[Mutation]:
        """Binary search by position; returns the Mutation or None."""
        i = self._index(pos)
        if i < len(self._muts) and self._muts[i].pos == pos:
            return self._muts[i]
        return None

    # -- modification --------------------------------------------------------
    def add(self, mutation: Union[str, Mutation]) -> None:
        """Insert a mutation at its sorted position.

        If the position is already occupied, the new mutation must continue
        from the existing one (new.par == existing.mut), otherwise ValueError.
        A reversion (new.mut == existing.par) removes the position entirely;
        any other continuation is merged into one net Mutation
        (A945G then G945T -> A945T).
        """
        if isinstance(mutation, str):
            mutation = Mutation.parse(mutation)
        i = self._index(mutation.pos)
        if i < len(self._muts) and self._muts[i].pos == mutation.pos:
            old = self._muts[i]
            if mutation.par != old.mut:
                raise ValueError(
                    f'{mutation} does not follow existing {old}: '
                    f'parental base {mutation.par} != {old.mut}')
            if mutation.mut == old.par:
                del self._muts[i]          # reversion: drop the position
            else:
                self._muts[i] = Mutation(old.par, old.pos, mutation.mut)
        else:
            self._muts.insert(i, mutation)

    def copy(self) -> 'MutationSet':
        """Independent copy (Mutations are immutable, so a shallow list copy suffices)."""
        new = MutationSet.__new__(MutationSet)
        new._muts = list(self._muts)
        return new

    __copy__ = copy

    def with_mutations(self, new_muts: Iterable[Union[str, Mutation]]) -> 'MutationSet':
        """Return a copy of self with new_muts added; self is not modified.

        new_muts is applied in order with the same rules as add(), so a
        ValueError partway through leaves self untouched.
        """
        new = self.copy()
        for m in new_muts:
            new.add(m)
        return new

    def has_reversion(self, new_muts: Iterable[Union[str, Mutation]]) -> bool:
        """True if any mutation in new_muts reverts a mutation already in self.

        A reversion is a mutation at the same position with the bases swapped
        (existing A945G, new G945A). Does not modify self. Each new mutation
        is compared against self only, not against others in new_muts.
        """
        for m in new_muts:
            if isinstance(m, str):
                m = Mutation.parse(m)
            old = self.find(m.pos)
            if old is not None and m.par == old.mut and m.mut == old.par:
                return True
        return False

    # -- container protocol --------------------------------------------------
    def __len__(self) -> int:
        return len(self._muts)

    def __iter__(self) -> Iterator[Mutation]:
        return iter(self._muts)

    def __getitem__(self, i):
        return self._muts[i]

    def __contains__(self, item) -> bool:
        if isinstance(item, str):
            item = Mutation.parse(item)
        return self.find(item.pos) == item

    def __eq__(self, other) -> bool:
        return isinstance(other, MutationSet) and self._muts == other._muts

    def to_strings(self) -> list[str]:
        return [str(m) for m in self._muts]

    def __repr__(self) -> str:
        return f'MutationSet({self.to_strings()!r})'


if __name__ == '__main__':
    ms = MutationSet(['C4567G', 'A945G', 'C254T'])
    assert ms.to_strings() == ['C254T', 'A945G', 'C4567G']   # sorted on init
    assert ms.find(945) == Mutation('A', 945, 'G')
    assert ms.find(946) is None
    assert 'C254T' in ms and 'C254G' not in ms

    orig = MutationSet(['C254T', 'A945G'])
    cp = orig.copy()
    cp.add('G945A')                                           # reversion
    assert cp.to_strings() == ['C254T']
    assert orig.to_strings() == ['C254T', 'A945G']            # original untouched

    cp.add('A100T'); cp.add('T100C')                          # chained at one position
    assert cp.to_strings() == ['A100C', 'C254T']
    try:
        cp.add('G254A')                                       # doesn't follow C254T
    except ValueError:
        pass
    else:
        raise AssertionError('expected ValueError')
    base = MutationSet(['C254T', 'A945G'])
    assert base.has_reversion(['G945A'])
    assert base.has_reversion(['A1T', Mutation('T', 254, 'C')])
    assert not base.has_reversion(['G945T', 'A1T', 'C254T'])  # no swap / no match
    assert not base.has_reversion([])
    nxt = base.with_mutations(['G945A', 'T7C'])
    assert nxt.to_strings() == ['T7C', 'C254T']
    assert base.to_strings() == ['C254T', 'A945G']            # self unchanged
    try:
        base.with_mutations(['T7C', 'G254A'])                 # fails on 2nd item
    except ValueError:
        assert base.to_strings() == ['C254T', 'A945G']
    else:
        raise AssertionError('expected ValueError')
    print('ok', cp)
