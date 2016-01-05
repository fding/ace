# ACE (A Chess Engine)
ACE is a chess engine that communicates via standard in and standard out. To run ACE, type

`./chess --white=human --black=computer`

on the command line.

## Compilation
Run `make all` to compile ACE and related binaries.

## Binaries
The following binaries are created: chess, perft, benchmark, score, generate_magic, and playself.

Chess is the main engine.

Perft runs perft (https://chessprogramming.wikispaces.com/Perft), which is used to test and benchmark the move generator. Running
`make test` will run perft against a selection of position and tabulated perft numbers (representing the number of nodes at depth d
reachable from a given position).

Benchmark runs the chess engine against itself with fixed search depth to benchmark move selection.

Score uses ACE's internal scoring function to score a board position.

Playself runs two chess engines that follows the same protocol as ACE against each other:

`./playself --white ./chess1 --black ./chess2`

These chess engines run in separate processes communicating via pipes. This is used to tune parameters and test different versions of ACE.

Finally, generate_magic is an internal binary used to generate some magic numbers for ACE.

## Protocol
ACE expects moves to be inputted via standard in and outputs its moves to standard out (and prints the board to standard error).
Moves basically follow the extended algebraic notation: e5e8 moves a piece on e5 to e8, capturing if necessary.
There are two extensions: for castling, specify the king's current square and the king's target, followed by the letter 'C', followed by
the square of the rook, like 'e1g1Ch1'. For promotions, follow the move by the piece to promote to, like 'e7e8Q'.

## Capabilities
ACE should follow all the rules of chess (although there are some rare bugs in the move generation to be rooted out),
including castling, en passant, 3-fold repetitions, and 50 move draws.
It uses an iterative deepening framework with an adjustible maximum thinking time (default 7 seconds),
using an alpha beta search that exhaustly analyzes move combinations to at least depth 6
(~12-14 in the endgame; it also considers "interesting" moves like captures and checks to up to depth 8-12 in the middle game).
It has a small opening book and maintains a large in memory transposition table to speed up move searching.
Its endgame ability is fairly lacking, however.

## Internals
ACE uses bitboards to represent pieces.
There is a separate 64 bit integer for each of the 12 color/piece combinations (black rooks, white rooks, etc).
Knight and king attacks are generated via lookup tables,
and pawn attacks are generated via bit shifting.
Rook, bishop, and queen attacks (i.e. sliding pieces) are generated using the magic bitboard algorithm:
for each square,
we construct a perfect hash function mapping all combinations of blocking pieces to an 7-12 bit integer,
which we use to index into a look up table to obtain the legal moves for the piece.
This hash function involves multiplying by a magic number, which is generated by `./generate_magic`.
Move generation uses two stages.
The first stage generates pseudo-legal moves.
These moves are always legal if the king is in check,
and they never move a king into check;
however, some of these moves might move a pinned piece out of the pin.
The second stage thus eliminates these moves to generate a set of completely legal moves.
ACE's move generation is reasonably efficient:
it can generate about 10 million moves per second on my 8 GB machine with Intel 2.6 GHz Intel Core i5.

Board scoring considers material advantage, piece positions (knights are better in the center of the board,
and the king is better away from the center until the endgame), pawn structure (bonuses for passed pawns, penalties for isolated pawns
and double pawns), positional factors (double bishops are good,
bishops are weaker when there are same colored pawns in the center of the board,
rooks are good on open and semiopen files, doubled rooks are good, and rooks are strong on the 7-th rank, etc),
and king safety (castling is good, pawns in front of kings are good, enemy pawns marching towards the king is bad,
kings with little move options are bad, kings in locations surrounded by opponent controlled squares are bad).

Move searching does an alpha beta search to find the best moves. It caches the scores for each board position, along
with the optimal move found (this is stored in the transposition table, which is indexed by the Zobrist hash of the board).
The moves are considered in order to try to maximize pruning:
we first consider stored moves (these are results of a full alpha beta search so are probably the best moves),
then check giving moves,
then valuable captures and promotions ,
then killer moves (moves that are good in sibling nodes; usually these moves are still good now).
It does null pruning and futility pruning to reduce the number of considered nodes.
Finally, at the end of the search, it does a quiescent search to explore to further depth captures and check giving moves,
to control for the horizon effect.
It further extends the search if it senses that the number of legal moves is small (perhaps indicating an imminent checkmate).