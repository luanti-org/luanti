#include <memory>
#include <vector>
#include "mapnode.h"

// Like a std::unordered_map<content_t, content_t>, but faster.
//
// Unassigned entries are marked with 0xFFFF.
//
// The static memory requires about 65535 * 2 bytes RAM in order to be
// sure we can handle all content ids.
class IdIdMapping
{
	static_assert(sizeof(content_t) == 2, "content_t must be 16-bit");

private:
	std::unique_ptr<content_t[]> m_mapping;
	std::vector<content_t> m_dirty;

public:
	IdIdMapping()
	{
		m_mapping = std::make_unique<content_t[]>(CONTENT_MAX + 1);
		memset(m_mapping.get(), 0xFF, (CONTENT_MAX + 1) * sizeof(content_t));
	}

	DISABLE_CLASS_COPY(IdIdMapping)

	content_t get(content_t k) const
	{
		return m_mapping[k];
	}

	void set(content_t k, content_t v)
	{
		m_mapping[k] = v;
		m_dirty.push_back(k);
	}

	void clear()
	{
		for (auto k : m_dirty)
			m_mapping[k] = 0xFFFF;
		m_dirty.clear();
	}

	static IdIdMapping &giveClearedThreadLocalInstance()
	{
		static thread_local IdIdMapping tl_ididmapping;
		tl_ididmapping.clear();
		return tl_ididmapping;
	}
};
