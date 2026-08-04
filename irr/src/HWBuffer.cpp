#include "HWBuffer.h"
#include "os.h"

namespace scene
{

void HWBuffer::setDirty() {
	++ChangedID;
	if constexpr (DEBUG) {
		if (MappingHint == EHM_STATIC && Link) {
			char buf[100];
			snprintf(buf, sizeof(buf), "HWBuffer @ %p modified, but it has a static hint", this);
			os::Printer::log(buf, ELL_WARNING);
		}
	}
}

} // end namespace scene
