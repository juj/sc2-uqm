typedef int uint32;

typedef struct {
	uint32 res;
	char *name;
} ResourceItem;

typedef struct {
	size_t numItems;
	ResourceItem *item;
} ResourceItemList;

