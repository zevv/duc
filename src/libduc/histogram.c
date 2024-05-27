
#include <math.h>

#include "duc.h"
#include "private.h"


duc_histogram *duc_histogram_new(duc_size_type st, size_t bin_min, size_t bin_max, double power)
{
	duc_histogram *h = duc_malloc(sizeof(duc_histogram));
	h->st = st;
	h->bins = logf(bin_max / bin_min) / logf(power) + 1;
	h->bin = duc_malloc(sizeof(struct duc_histogram_bin) * h->bins);

	// Always have a 0-size bin

	h->bin[0].min = 0;
	h->bin[0].max = 0;
	h->bin[0].file_count = 0;
	h->bin[0].dir_count = 0;

	// Create the rest of the bins

	for(size_t i=0; i<h->bins-1; i++) {
		h->bin[i+1].min = bin_min * powf(power, i);
		h->bin[i+1].max = bin_min * powf(power, i+1);
		h->bin[i+1].file_count = 0;
		h->bin[i+1].dir_count = 0;
	}

	// TODO: gap between bin[0] and bin[1] is not handled
	// TODO: > bin_max is not handled

	return h;
}


void duc_histogram_accumumlate(duc_histogram *h, struct duc_dirent *e)
{
	if(h == NULL) {
		return;
	}

	/* For now, do a naive linear search instead of calculating
	 * the bin number. Fast enough until proven otherwise */

	for(size_t i=0; i<h->bins; i++) {
		size_t size = h->st == DUC_SIZE_TYPE_ACTUAL ? e->size.actual : e->size.apparent;
		if(size >= h->bin[i].min && size < h->bin[i].max) {
			if(e->type == DUC_FILE_TYPE_DIR) {
				h->bin[i].dir_count++;
			} else {
				h->bin[i].file_count++;
			}
			break;
		}
	}
}


void duc_histogram_free(duc_histogram *h)
{
	duc_free(h->bin);
	duc_free(h);
}

