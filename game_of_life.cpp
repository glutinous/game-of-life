#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#endif

struct Cell
{
    int64_t cellX = 0;
    int64_t cellY = 0;
};

// The maximum length of a 64-bit signed int is 20 characters, so this should be
// more than enough to handle two separated by a space.
constexpr int LINE_MAX = 256;

constexpr int CELLS_MAX = 1024 * 1024;
constexpr int SLOTS_MAX = CELLS_MAX * 3;

// This is either 0 or 1. For toggling between previous and next states of the
// simulation.
static int s_currentGenerationIndex = 0;

// List of all alive cells. Ping-pong between two sets of these as we evaluate
// each new generation.
static Cell s_allCells[2][CELLS_MAX];
static int s_allCellsCount[2] = { 0 };

// Cell positions will be hashed and stored in slots using
// closed-hashing/open-addressing to resolve collisions.
//
// Ping-pong between two sets of slots as we evaluate each generation/step of
// the simulation.
static int s_allCellSlots[2][SLOTS_MAX];

static uint32_t HashFunc(int64_t x, int64_t y)
{
    // Murmur hash

    uint64_t h = static_cast<uint64_t>(x);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;

    h *= static_cast<uint64_t>(y);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;

    const uint32_t rv = static_cast<uint32_t>(h);
    return rv;
}

static int FindNextFreeSlot(int generationIndex, int initialIndex)
{
    for (int i = 0; i < SLOTS_MAX; ++i)
    {
        const int slotIndex = (initialIndex + i) % SLOTS_MAX;
        assert(slotIndex >= 0);
        assert(slotIndex < SLOTS_MAX);
        if (s_allCellSlots[generationIndex][slotIndex] < 0)
        {
            return slotIndex;
        }
    }

    puts("No free slots found");
    exit(-1);
}

static bool IsCellAlive(int generationIndex, int64_t x, int64_t y)
{
    assert((generationIndex == 0) || (generationIndex == 1));
    const uint32_t hash = HashFunc(x, y);
    const int initialIndex = hash % SLOTS_MAX;
    for (int i = 0; i < SLOTS_MAX; ++i)
    {
        const int slotIndex = (initialIndex + i) % SLOTS_MAX;
        assert(slotIndex >= 0);
        assert(slotIndex < SLOTS_MAX);

        const int cellIndex = s_allCellSlots[generationIndex][slotIndex];
        if (cellIndex < 0)
            return false;

        assert(cellIndex >= 0);
        assert(cellIndex < CELLS_MAX);
        assert(cellIndex < s_allCellsCount[generationIndex]);

        const Cell &cell = s_allCells[generationIndex][cellIndex];
        if (cell.cellX == x)
        {
            if (cell.cellY == y)
            {
                return true;
            }
        }
    }
    return false;
}

static int CountAliveNeighbors(int generationIndex, int64_t x, int64_t y)
{
    int aliveNeighbors = 0;

    const int64_t xMin = (x == INT64_MIN) ? x : x - 1;
    const int64_t xMax = (x == INT64_MAX) ? x : x + 1;

    const int64_t yMin = (y == INT64_MIN) ? y : y - 1;
    const int64_t yMax = (y == INT64_MAX) ? y : y + 1;

    for (int64_t yNeighbor = yMin; yNeighbor <= yMax; ++yNeighbor)
    {
        for (int64_t xNeighbor = xMin; xNeighbor <= xMax; ++xNeighbor)
        {
            if ((xNeighbor == x) && (yNeighbor == y))
                continue;

            if (IsCellAlive(generationIndex, xNeighbor, yNeighbor))
            {
                ++aliveNeighbors;
            }
        }
    }

    return aliveNeighbors;
}

static void AddCell(int generationIndex, int64_t x, int64_t y)
{
    assert((generationIndex == 0) || (generationIndex == 1));

    if (IsCellAlive(generationIndex, x, y))
        return;

    const int cellIndex = s_allCellsCount[generationIndex];
    assert(cellIndex >= 0);
    assert(cellIndex < CELLS_MAX);
    ++s_allCellsCount[generationIndex];

    Cell& cell = s_allCells[generationIndex][cellIndex];
    cell.cellX = x;
    cell.cellY = y;

    const uint32_t hash = HashFunc(x, y);
    const int initialIndex = hash % SLOTS_MAX;
    const int slotIndex = FindNextFreeSlot(generationIndex, initialIndex);
    assert(slotIndex >= 0);
    assert(slotIndex < SLOTS_MAX);
    assert(s_allCellSlots[generationIndex][slotIndex] == -1);
    s_allCellSlots[generationIndex][slotIndex] = cellIndex;
}

static void Populate()
{
    // Initialize all s_allCellSlots elements to '-1'. '-1' as a 32-bit int is
    // 0xffffffff. So just setting every byte to 0xff will work too.
    memset(s_allCellSlots, -1, sizeof(s_allCellSlots));

    char buffer[LINE_MAX];

    gets_s(buffer, sizeof(buffer));
    if (strcmp(buffer, "#Life 1.06"))
    {
        printf("Invalid header: %s\n", buffer);
        exit(-1);
    }

    while (true)
    {
        char* const getsResult = gets_s(buffer, sizeof(buffer));
        if (!getsResult)
        {
            if (feof(stdin))
            {
                break;
            }
            else
            {
                puts("Error reading stdin");
                exit(-1);
            }
        }

        int64_t x = 0;
        int64_t y = 0;
        const int scanResult = sscanf_s(buffer, "%" SCNd64 " %" SCNd64, &x, &y);
        if (scanResult == EOF)
            break;

        if (scanResult != 2)
        {
            printf("Error scanning line: %s\n", buffer);
            exit(-1);
        }

        assert(s_currentGenerationIndex == 0);

        if (s_allCellsCount[0] >= CELLS_MAX)
        {
            puts("Maximum number of cells reached");
            exit(-1);
        }

        AddCell(0, x, y);
    }
}

static void KillCells(int genPrev, int genNext)
{
    for (int i = 0; i < s_allCellsCount[genPrev]; ++i)
    {
        const Cell& cell = s_allCells[genPrev][i];
        const int aliveCount = CountAliveNeighbors(genPrev, cell.cellX, cell.cellY);
        if (aliveCount >= 2)
        {
            if (aliveCount <= 3)
            {
                // Something is wrong if we are adding a cell to the same
                // location twice at this point.
                assert(!IsCellAlive(genNext, cell.cellX, cell.cellY));

                // This cell is still alive; add it to the next generation.
                AddCell(genNext, cell.cellX, cell.cellY);
            }
        }
    }
}

static void GrowCells(int genPrev, int genNext)
{
    for (int i = 0; i < s_allCellsCount[genPrev]; ++i)
    {
        const Cell& cell = s_allCells[genPrev][i];
        assert(IsCellAlive(genPrev, cell.cellX, cell.cellY));

        const int64_t xMin = (cell.cellX == INT64_MIN) ? cell.cellX : cell.cellX - 1;
        const int64_t xMax = (cell.cellX == INT64_MAX) ? cell.cellX : cell.cellX + 1;

        const int64_t yMin = (cell.cellY == INT64_MIN) ? cell.cellY : cell.cellY - 1;
        const int64_t yMax = (cell.cellY == INT64_MAX) ? cell.cellY : cell.cellY + 1;

        for (int64_t yNeighbor = yMin; yNeighbor <= yMax; ++yNeighbor)
        {
            for (int64_t xNeighbor = xMin; xNeighbor <= xMax; ++xNeighbor)
            {
                if ((xNeighbor == cell.cellX) && (yNeighbor == cell.cellY))
                    continue;

                // Only care about dead neighbors
                if (IsCellAlive(genPrev, xNeighbor, yNeighbor))
                    continue;

                const int aliveCount = CountAliveNeighbors(genPrev, xNeighbor, yNeighbor);
                if (aliveCount)
                {
                    AddCell(genNext, xNeighbor, yNeighbor);
                }
            }
        }
    }
}

static void Step()
{
    assert((s_currentGenerationIndex == 0) || (s_currentGenerationIndex == 1));
    const int genPrev = s_currentGenerationIndex;
    const int genNext = (s_currentGenerationIndex == 0) ? 1 : 0;

    // Create new list for the next generation of cells.
    s_allCellsCount[genNext] = 0;

    // Initialize all elements of the the next generation of s_allCellSlots to
    // '-1'.
    memset(s_allCellSlots + genNext, -1, sizeof(s_allCellSlots[0]));

    KillCells(genPrev, genNext);
    GrowCells(genPrev, genNext);

    s_currentGenerationIndex = genNext;
}

static void PrintResults()
{
    puts("#Life 1.06");

    char buffer[LINE_MAX];

    for (int i = 0; i < s_allCellsCount[s_currentGenerationIndex]; ++i)
    {
        const Cell& cell = s_allCells[s_currentGenerationIndex][i];

        const int printResult = snprintf(buffer, sizeof(buffer), "%" PRId64 " %" PRId64, cell.cellX, cell.cellY);
        if (printResult < 0)
        {
            puts("Error printing result");
            exit(-1);
        }

        puts(buffer);

#ifdef _WIN32
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
#endif
    }
}

int main()
{
    Populate();

    for (int i = 0; i < 10; ++i)
    {
        Step();
    }

    PrintResults();

    return 0;
}