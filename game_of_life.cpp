#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// For toggling between previous and next states of the simulation.
static int s_allCellSlotsActiveIndex = 0;

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

static int FindNextFreeSlot(int initialIndex)
{
    for (int i = 0; i < SLOTS_MAX; ++i)
    {
        const int slotIndex = (initialIndex + i) % SLOTS_MAX;
        assert(slotIndex >= 0);
        assert(slotIndex < SLOTS_MAX);
        if (s_allCellSlots[s_allCellSlotsActiveIndex][slotIndex] < 0)
        {
            return slotIndex;
        }
    }

    puts("No free slots found");
    exit(-1);
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

        assert(s_allCellSlotsActiveIndex == 0);

        if (s_allCellsCount[0] >= CELLS_MAX)
        {
            puts("Maximum number of cells reached");
            exit(-1);
        }

        const int cellIndex = s_allCellsCount[0];
        assert(cellIndex >= 0);
        assert(cellIndex < CELLS_MAX);
        ++s_allCellsCount[0];

        Cell& cell = s_allCells[0][cellIndex];
        cell.cellX = x;
        cell.cellY = y;

        const uint32_t hash = HashFunc(x, y);
        const int initialIndex = hash % SLOTS_MAX;
        const int slotIndex = FindNextFreeSlot(initialIndex);
        assert(slotIndex >= 0);
        assert(slotIndex < SLOTS_MAX);
        assert(s_allCellSlots[0][slotIndex] == -1);
        s_allCellSlots[0][slotIndex] = cellIndex;
    }
}

static void Step()
{
    // Initialize all elements of the the next generation of s_allCellSlots to
    // '-1'.
    assert((s_allCellSlotsActiveIndex == 0) || (s_allCellSlotsActiveIndex == 1));
    const int genPrev = s_allCellSlotsActiveIndex;
    const int genNext = (s_allCellSlotsActiveIndex == 0) ? 1 : 0;
    memset(s_allCellSlots + genNext, -1, sizeof(s_allCellSlots[0]));
}

int main()
{
    Populate();

    for (int i = 0; i < 10; ++i)
    {
        Step();
    }

    return 0;
}