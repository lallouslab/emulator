#include "std_include.hpp"

#define X86_CODE32 "\x65\x48\x8B\x04\x25\x60\x00\x00\x00" // INC ecx; DEC edx
#define ADDRESS 0x1000000

#define GS_SEGMENT_ADDR 0x6000000ULL
#define GS_SEGMENT_SIZE (20 << 20)  // 20 MB

#define IA32_GS_BASE_MSR 0xC0000101

#define STACK_SIZE 0x40000

#include "unicorn.hpp"

namespace
{
	uint64_t align_down(const uint64_t value, const uint64_t alignment)
	{
		return value & ~(alignment - 1);
	}

	uint64_t align_up(const uint64_t value, const  uint64_t alignment)
	{
		return align_down(value + (alignment - 1), alignment);
	}

	void setup_gs_segment(const unicorn& uc, const uint64_t segment_base, const size_t size)
	{
		const std::array<uint64_t, 2> value = {
			IA32_GS_BASE_MSR,
			segment_base
		};

		e(uc_reg_write(uc, UC_X86_REG_MSR, value.data()));
		e(uc_mem_map(uc, segment_base, size, UC_PROT_READ | UC_PROT_WRITE));
	}

	void setup_teb_and_peb(const unicorn& uc)
	{
		setup_gs_segment(uc, GS_SEGMENT_ADDR, GS_SEGMENT_SIZE);

		constexpr auto teb_address = GS_SEGMENT_ADDR;
		const auto peb_address = align_up(teb_address + sizeof(TEB), 0x10);

		TEB teb{};
		teb.NtTib.Self = reinterpret_cast<NT_TIB*>(teb_address);
		teb.ProcessEnvironmentBlock = reinterpret_cast<PEB*>(peb_address);


		PEB peb{};

		e(uc_mem_write(uc, teb_address, &teb, sizeof(teb)));
		e(uc_mem_write(uc, peb_address, &peb, sizeof(peb)));
	}

	void run()
	{
		const unicorn uc{UC_ARCH_X86, UC_MODE_64};

		e(uc_mem_map(uc, ADDRESS, 0x1000, UC_PROT_ALL));
		e(uc_mem_write(uc, ADDRESS, X86_CODE32, sizeof(X86_CODE32) - 1));


		setup_teb_and_peb(uc);

		e(uc_emu_start(uc, ADDRESS, ADDRESS + sizeof(X86_CODE32) - 1, 0, 0));

		printf("Emulation done. Below is the CPU context\n");

		uint64_t rax{};
		e(uc_reg_read(uc, UC_X86_REG_RAX, &rax));

		printf(">>> RAX = 0x%llX\n", rax);
	}
}

int main(int /*argc*/, char** /*argv*/)
{
	try
	{
		run();
		return 0;
	}
	catch (std::exception& e)
	{
		puts(e.what());

#ifdef _WIN32
		MessageBoxA(nullptr, e.what(), "ERROR", MB_ICONERROR);
#endif
	}

	return 1;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
	return main(__argc, __argv);
}
#endif