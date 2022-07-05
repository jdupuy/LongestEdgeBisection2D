struct cct_Bisector {
    uint id;
    int depth;
};
#define cct_BisectorHalfedgeIDs uvec3
#define cct_BisectorNeighborIDs ivec3

// diamond parent
struct cct_DiamondParent{
    cbt_Node base, top;
};
cct_DiamondParent cct_DecodeDiamondParent(const in cbt_Node node);

// accessors
int cct_RootBisectorCount();
int cct_BisectorCount(const int cbtID);

// conversion routines
cct_Bisector cct_NodeToBisector(const in cbt_Node node);
cbt_Node cct_BisectorToNode(const in cct_Bisector bisector);

// decoding routines
cct_BisectorHalfedgeIDs cct_DecodeHalfedgeIDs(cct_Bisector bisector);
cct_BisectorNeighborIDs cct_DecodeNeighborIDs(cct_Bisector bisector);

// split / merge
void cct_Split(const int cbtID,
               const cct_Bisector bisector);
void cct_MergeNode(const int cbtID,
                   const in cbt_Node node,
                   const in cct_DiamondParent diamond);

//
//
//// end header file ///////////////////////////////////////////////////////////


/*******************************************************************************
 * LgPowerOfTwo -- Find the log base 2 of an integer that is a power of two
 *
 * See:
 * https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
 *
 */
uint cct__LgPowerOfTwo(uint x)
{
    uint r = uint((x & 0xAAAAAAAAu) != 0u);

    r |= uint((x & 0xFFFF0000u) != 0) << 4;
    r |= uint((x & 0xFF00FF00u) != 0) << 3;
    r |= uint((x & 0xF0F0F0F0u) != 0) << 2;
    r |= uint((x & 0xCCCCCCCCu) != 0) << 1;

    return r;
}


/*******************************************************************************
 * RoundUpToPowerOfTwo -- Computes the next highest power of 2 for a 32-bit integer
 *
 * See:
 * https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
 *
 */
uint cct__RoundUpToPowerOfTwo(uint x)
{
    x--;
    x|= x >>  1;
    x|= x >>  2;
    x|= x >>  4;
    x|= x >>  8;
    x|= x >> 16;
    x++;

    return x;
}


/*******************************************************************************
 * RootBisectorCount -- returns the number of faces at root
 *
 */
int cct_RootBisectorCount()
{
    return ccm_HalfedgeCount();
}


/*******************************************************************************
 * MinCbtDepth -- returns the minimum depth required for the CBT to work
 *
 */
int cct__MinCbtDepth()
{
    const uint rootBisectorCount = uint(cct_RootBisectorCount());

    return int(cct__LgPowerOfTwo(cct__RoundUpToPowerOfTwo(rootBisectorCount)));
}


/*******************************************************************************
 * NullBisectorCount -- returns the number of unused bisectors created in the CBT
 *
 */
int cct__NullBisectorCount()
{
    return (1 << cct__MinCbtDepth()) - cct_RootBisectorCount();
}


/*******************************************************************************
 * MaxBisectorDepth -- returns the maximum bisection depth supported by the subd
 *
 */
int cct__MaxBisectorDepth()
{
    return (ccs_MaxDepth() << 1) - 1;
}


/*******************************************************************************
 * BisectorCount -- Returns the number of bisectors in the tessellation
 *
 */
int cct_BisectorCount(const int cbtID)
{
    return int(cbt_NodeCount(cbtID)) - cct__NullBisectorCount();
}


/*******************************************************************************
 * NodeToBisector -- Converts a cbt_Node to a cct_Bisector
 *
 */
cct_Bisector cct_NodeToBisector(const cbt_Node node)
{
    const int depth = node.depth - cct__MinCbtDepth();
    const uint bitMask = 1u << node.depth;

    return cct_Bisector(node.id ^ bitMask, depth);
}


/*******************************************************************************
 * BisectorToNode -- Converts a cct_Bisector to a cbt_Node
 *
 */
cbt_Node
cct_BisectorToNode_Fast(const cct_Bisector bisector, const int minCbtDepth)
{
    const int depth = bisector.depth + minCbtDepth;
    const uint bitMask = 1u << depth;

    return cbt_Node(bisector.id | bitMask, depth);
}
cbt_Node cct_BisectorToNode(const cct_Bisector bisector)
{
    return cct_BisectorToNode_Fast(bisector, cct__MinCbtDepth());
}


/*******************************************************************************
 * GetBitValue -- Returns the value of a bit stored in a 32-bit word
 *
 */
uint cct__GetBitValue(const uint bitField, int bitID)
{
    return ((bitField >> bitID) & 1u);
}


/*******************************************************************************
 * EvenRule -- Even half-edge splitting rule
 *
 */
void cct__EvenRule(inout uvec3 x, uint b)
{
    if (b == 0u) {
        x[2] = (x[0] | 2u);
        x[1] = (x[0] | 1u);
        x[0] = (x[0] | 0u);
    } else {
        x[0] = (x[2] | 2u);
        x[1] = (x[2] | 3u);
        x[2] = (x[2] | 0u);
    }
}


/*******************************************************************************
 * EvenRule -- Odd half-edge splitting rule
 *
 */
void cct__OddRule(inout uvec3 x, uint b)
{
    if (b == 0u) {
        x[2] = (x[1] << 2) | 0u;
        x[1] = (x[0] << 2) | 2u;
        x[0] = (x[0] << 2) | 0u;
    } else {
        x[0] = (x[1] << 2) | 0u;
        x[1] = (x[1] << 2) | 2u;
        x[2] = (x[2] << 2) | 0u;
    }
}


/*******************************************************************************
 * DecodeHalfedgeIDs -- Retrieve the halfedges associated with the bisector
 *
 */
cct_BisectorHalfedgeIDs cct_DecodeHalfedgeIDs(cct_Bisector bisector)
{
    const int halfedgeID = int(bisector.id >> bisector.depth);
    const int nextID = ccm_HalfedgeNextID(halfedgeID);
    const int h0 = 4 * halfedgeID + 0;
    const int h1 = 4 * halfedgeID + 2;
    const int h2 = 4 * nextID;
    cct_BisectorHalfedgeIDs halfedgeIDs = cct_BisectorHalfedgeIDs(h0, h1, h2);
    bool isEven = true;

    for (int bitID = bisector.depth - 1; bitID >= 0; --bitID) {
        const uint bitValue = cct__GetBitValue(bisector.id, bitID);

        if (isEven) {
            cct__EvenRule(halfedgeIDs, bitValue);
        } else {
            cct__OddRule(halfedgeIDs, bitValue);
        }

        isEven = !isEven;
    }

    // swap winding for even levels
    if (isEven) {
        uint tmp = halfedgeIDs[0];

        halfedgeIDs[0] = halfedgeIDs[2];
        halfedgeIDs[2] = tmp;
    }

    return halfedgeIDs;
}


/*******************************************************************************
 * BisectNeighborIDs -- Computes new neighborhood after one subdivision step
 *
 */
cct_BisectorNeighborIDs
cct__BisectNeighborIDs(
    const cct_BisectorNeighborIDs neighborIDs,
    const int bisectorID,
    uint bitValue
) {
    const int b = bisectorID;
    const int n0 = neighborIDs.x, n1 = neighborIDs.y, n2 = neighborIDs.z;

    if (bitValue == 0) {
        return cct_BisectorNeighborIDs(2 * n2 + 1, 2 * b + 1, 2 * n0 + 1);
    } else {
        return cct_BisectorNeighborIDs(2 * n1 + 0, 2 * n0 + 0, 2 * b + 0);
    }
}


/*******************************************************************************
 * DecodeNeighborIDs -- Computes bisector neighborhood
 *
 */
cct_BisectorNeighborIDs cct_DecodeNeighborIDs(cct_Bisector bisector)
{
    const int halfedgeID = int(bisector.id >> bisector.depth);
    const int n0 = ccm_HalfedgeTwinID(halfedgeID);
    const int n1 = ccm_HalfedgeNextID(halfedgeID);
    const int n2 = ccm_HalfedgePrevID(halfedgeID);
    cct_BisectorNeighborIDs neighborIDs = cct_BisectorNeighborIDs(n0, n1, n2);

    for (int bitID = bisector.depth - 1; bitID >= 0; --bitID) {
        const int bisectorID = int(bisector.id >> bitID);

        neighborIDs = cct__BisectNeighborIDs(neighborIDs,
                                             bisectorID >> 1,
                                             bisectorID & 1);
    }

    return neighborIDs;
}


/*******************************************************************************
 * SplitNode -- Bisects a triangle in the current tessellation
 *
 */
void cct_Split(const int cbtID, const cct_Bisector bisector)
{
    const int minCbtDepth = cct__MinCbtDepth();
    cct_Bisector iterator = bisector;

    cbt_SplitNode_Fast(cbtID, cct_BisectorToNode(iterator));
    iterator.id = cct_DecodeNeighborIDs(iterator).x;

    while (iterator.id >= 0 && iterator.depth >= 0) {
        cbt_SplitNode_Fast(cbtID, cct_BisectorToNode_Fast(iterator, minCbtDepth));
        iterator.id>>= 1; iterator.depth-= 1; // parent bisector
        cbt_SplitNode_Fast(cbtID, cct_BisectorToNode_Fast(iterator, minCbtDepth));
        iterator.id = cct_DecodeNeighborIDs(iterator).x;
    }
}


/*******************************************************************************
 * DecodeDiamondParent -- Decodes the diamond associated to the Node
 *
 * If the neighbour part does not exist, the parentNode is copied instead.
 *
 */
cct_DiamondParent cct_DecodeDiamondParent(in const cbt_Node node)
{
    const cbt_Node parentNode = cbt_ParentNode_Fast(node);
    const cct_Bisector bisector = cct_NodeToBisector(parentNode);
    const int edgeTwinID = cct_DecodeNeighborIDs(bisector).x;
    const int bitMask = 1 << parentNode.depth;
    const cbt_Node edgeNeighborNode = cbt_CreateNode(
        edgeTwinID > 0 ? (bitMask | edgeTwinID) : parentNode.id,
        parentNode.depth
    );

    return cct_DiamondParent(parentNode, edgeNeighborNode);
}


/*******************************************************************************
 * HasDiamondParent -- Determines whether a diamond parent is actually stored
 *
 * This procedure checks that the diamond parent is encoded in the CBT.
 * We can perform this test by checking that both the base and top nodes
 * that form the diamond parent are split, i.e., CBT[base] = CBT[top] = 2.
 * This is a crucial operation for implementing the leb_Merge routine.
 *
 */
bool
cct__HasDiamondParent(
    const int cbtID,
    const in cct_DiamondParent diamondParent
) {
    const bool canMergeBase = cbt_HeapRead(cbtID, diamondParent.base) <= 2u;
    const bool canMergeTop  = cbt_HeapRead(cbtID, diamondParent.top) <= 2u;

    return canMergeBase && canMergeTop;
}


/*******************************************************************************
 * MergeNode -- Bisects a triangle in the current tessellation
 *
 */
bool cct_IsRootNode(const in cbt_Node node)
{
    return node.depth == cct__MinCbtDepth();
}

void
cct_MergeNode(
    const int cbtID,
    in const cbt_Node node,
    in const cct_DiamondParent diamondParent
) {
    if (!cct_IsRootNode(node) && cct__HasDiamondParent(cbtID, diamondParent)) {
        cbt_MergeNode(cbtID, node);
    }
}
