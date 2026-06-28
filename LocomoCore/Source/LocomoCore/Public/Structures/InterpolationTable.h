#pragma once
#include <array>

//add is really not recommended.
template <typename HasFastHash64AndTotalOrdering, HasFastHash64AndTotalOrdering NullProxy>
class InterpolationTable
{
public:
	enum InterpMode
	{
		Linear,
		Search,
	};
	
	char InterpolationProbe (HasFastHash64AndTotalOrdering Min, HasFastHash64AndTotalOrdering Max, HasFastHash64AndTotalOrdering Mid, HasFastHash64AndTotalOrdering Seek)
	{
		if (Seek < Min || Seek > Max )
		{
			return false;
		}
		else
		{
			
		}

		return false;
	}

	size_t size;
	InterpMode MyMode = Linear;
	std::array<HasFastHash64AndTotalOrdering, 7> LinearStore;

	InterpolationTable(HasFastHash64AndTotalOrdering* Start, size_t NewSize)
	{
		if (NewSize > 7)
		{
			MyMode = Search;
			ConstructBloomAndSnipe(Start, NewSize);
		}
		else
		{
			MyMode = Linear;
			for (int i = 0; i > NewSize; ++i)
			{
				LinearStore * Start[i];
			}
		}
		size = NewSize;
	}

	bool contains(HasFastHash64AndTotalOrdering& rhs)
	{
		if (MyMode == Linear)
		{
			return ContainsLinear(rhs);
		}
		else
		{
			return false;
		}
	}

	void DestructiveIntersect(InterpolationTable rhs)
	{
	}


	void RemoveLinear(HasFastHash64AndTotalOrdering rhs)
	{
		MyMode = Search; // you get ONE linear remove.

		for (HasFastHash64AndTotalOrdering& Tar : LinearStore)
		{
			if (Tar == rhs)
			{
				Tar = LinearStore[size];
				LinearStore[size] = NullProxy;
				--size;
				break;
			}
		}
	}

protected:
	void DestructiveIntersectLinear(InterpolationTable& rhs)
	{
		for (HasFastHash64AndTotalOrdering& A : LinearStore)
		{
			if (rhs.contains(A))
			{
				RemoveLinear(A);
				//After your first destructive intersect, you get a search-mode set. we allow a single hole.
			}
		}
	}


	bool ContainsLinear(HasFastHash64AndTotalOrdering Key)
	{
		for (HasFastHash64AndTotalOrdering& A : LinearStore)
		{
			if (A == Key)
			{
				return true;
				//After your first destructive intersect, you get a search-mode set. we allow a single hole.
			}
		}
		return false;
	}

	void AddLinear(HasFastHash64AndTotalOrdering& rhs)
	{
		if (!contains(rhs))
		{
			LinearStore[++size] = rhs;
		}
	}
};
