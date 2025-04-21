#pragma once
#include "Disassembler.h"
#include "VarValue.h"

// forward declaration
struct AsmText;
struct FnParams;

class ILInstr {
public:
	enum ILKind {
		Unknown = 0,
		EnterFrame,
		LeaveFrame,
		AllocateStack,
		CheckStackOverflow,
		CallLeafRuntime,
		LoadValue,
		ClosureCall,
		MoveReg,
		DecompressPointer,
		SaveRegister,
		RestoreRegister,
		SetupParameters,
		InitAsync,
		GdtCall,
		Call,
		Return,
		BranchIfSmi,
		LoadClassId,
		LoadTaggedClassIdMayBeSmi,
		BoxInt64,
		LoadInt32,
		AllocateObject,
		LoadArrayElement,
		StoreArrayElement,
		LoadField,
		StoreField,
		InitLateStaticField,
		LoadStaticField,
		StoreStaticField,
		WriteBarrier,
		TestType,
	};

	ILInstr(const ILInstr&) = delete;
	ILInstr(ILInstr&&) = delete;
	ILInstr& operator=(const ILInstr&) = delete;
	virtual ~ILInstr() {}

	virtual std::string ToString() = 0;
	ILKind Kind() const { return kind; }
	uint64_t Start() const { return addrRange.start; }
	uint64_t End() const { return addrRange.end; }
	AddrRange Range() const { return addrRange; }

protected:
	ILInstr(ILKind kind, AddrRange& addrRange) : addrRange(addrRange), kind(kind) {}
	ILInstr(ILKind kind, cs_insn* insn) : addrRange(insn->address, insn->address + insn->size), kind(kind) {}

	AddrRange addrRange;
	ILKind kind;
};

class UnknownInstr : public ILInstr {
public:
	UnknownInstr(cs_insn* insn, AsmText& asm_text) : ILInstr(Unknown, insn), asm_text(asm_text) {}
	UnknownInstr() = delete;
	UnknownInstr(UnknownInstr&&) = delete;
	UnknownInstr& operator=(const UnknownInstr&) = delete;

	virtual std::string ToString() {
		return "unknown";
	}

protected:
	AsmText& asm_text;
};

class EnterFrameInstr : public ILInstr {
public:
	// 2 assembly instructions (stp lr, fp, [sp, 8]!; mov fp, sp)
	EnterFrameInstr(AddrRange addrRange) : ILInstr(EnterFrame, addrRange) {}
	EnterFrameInstr() = delete;
	EnterFrameInstr(EnterFrameInstr&&) = delete;
	EnterFrameInstr& operator=(const EnterFrameInstr&) = delete;

	virtual std::string ToString() {
		return "EnterFrame";
	}
};

class LeaveFrameInstr : public ILInstr {
public:
	LeaveFrameInstr(AddrRange addrRange) : ILInstr(LeaveFrame, addrRange) {}
	LeaveFrameInstr() = delete;
	LeaveFrameInstr(LeaveFrameInstr&&) = delete;
	LeaveFrameInstr& operator=(const LeaveFrameInstr&) = delete;

	virtual std::string ToString() {
		return "LeaveFrame";
	}
};

class AllocateStackInstr : public ILInstr {
public:
	AllocateStackInstr(AddrRange addrRange, uint32_t allocSize) : ILInstr(AllocateStack, addrRange), allocSize(allocSize) {}
	AllocateStackInstr() = delete;
	AllocateStackInstr(AllocateStackInstr&&) = delete;
	AllocateStackInstr& operator=(const AllocateStackInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("AllocStack({:#x})", allocSize);
	}
	uint32_t AllocSize() { return allocSize; }

protected:
	uint32_t allocSize;
};

class CheckStackOverflowInstr : public ILInstr {
public:
	// 3 assembly instructions (ldr tmp, [THR, stack_limit_offset]; cmp sp, tmp; b.ls #overflow_branch)
	CheckStackOverflowInstr(AddrRange addrRange, uint64_t overflowBranch) : ILInstr(CheckStackOverflow, addrRange), overflowBranch(overflowBranch) {}
	CheckStackOverflowInstr() = delete;
	CheckStackOverflowInstr(CheckStackOverflowInstr&&) = delete;
	CheckStackOverflowInstr& operator=(const CheckStackOverflowInstr&) = delete;

	virtual std::string ToString() {
		return "CheckStackOverflow";
	}

protected:
	uint64_t overflowBranch;
};

class MoveRegInstr : public ILInstr {
public:
	MoveRegInstr(AddrRange addrRange, A64::Register dstReg, A64::Register srcReg) : ILInstr(MoveReg, addrRange), dstReg(dstReg), srcReg(srcReg) {}
	MoveRegInstr() = delete;
	MoveRegInstr(MoveRegInstr&&) = delete;
	MoveRegInstr& operator=(const MoveRegInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = {}", dstReg.Name(), srcReg.Name());
	}

	A64::Register dstReg;
	A64::Register srcReg;
};

class CallLeafRuntimeInstr : public ILInstr {
public:
	CallLeafRuntimeInstr(AddrRange addrRange, uint64_t thrOffset) : ILInstr(CallLeafRuntime, addrRange), thrOffset(thrOffset) {}
	CallLeafRuntimeInstr(AddrRange addrRange, uint64_t thrOffset, std::vector<std::unique_ptr<MoveRegInstr>> ils) 
		: ILInstr(CallLeafRuntime, addrRange), thrOffset(thrOffset), movILs(std::move(ils)) {}
	CallLeafRuntimeInstr() = delete;
	CallLeafRuntimeInstr(CallLeafRuntimeInstr&&) = delete;
	CallLeafRuntimeInstr& operator=(const CallLeafRuntimeInstr&) = delete;

	virtual std::string ToString();

	int32_t thrOffset;
	std::vector<std::unique_ptr<MoveRegInstr>> movILs;
};

class LoadValueInstr : public ILInstr {
public:
	LoadValueInstr(AddrRange addrRange, A64::Register dstReg, VarItem val) : ILInstr(LoadValue, addrRange), dstReg(dstReg), val(std::move(val)) {}
	LoadValueInstr() = delete;
	LoadValueInstr(LoadValueInstr&&) = delete;
	LoadValueInstr& operator=(const LoadValueInstr&) = delete;

	virtual std::string ToString() {
		return std::string(dstReg.Name()) + " = " + val.Name();
	}

	VarItem& GetValue() {
		return val;
	}

	A64::Register dstReg;
	VarItem val;
};

class StoreObjectPoolInstr : public ILInstr {
public:
	StoreObjectPoolInstr(AddrRange addrRange, A64::Register srcReg, int64_t offset) : ILInstr(LoadValue, addrRange), srcReg(srcReg), offset(offset) {}
	StoreObjectPoolInstr() = delete;
	StoreObjectPoolInstr(StoreObjectPoolInstr&&) = delete;
	StoreObjectPoolInstr& operator=(const StoreObjectPoolInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("[PP+{:#x}] = {}", offset, srcReg.Name());
	}

	A64::Register srcReg;
	int64_t offset;
};

class ClosureCallInstr : public ILInstr {
public:
	ClosureCallInstr(AddrRange addrRange, int32_t numArg, int32_t numTypeArg) : ILInstr(ClosureCall, addrRange), numArg(numArg), numTypeArg(numTypeArg) {}
	ClosureCallInstr() = delete;
	ClosureCallInstr(ClosureCallInstr&&) = delete;
	ClosureCallInstr& operator=(const ClosureCallInstr&) = delete;

	virtual std::string ToString() {
		return "ClosureCall";
	}

	int32_t numArg;
	int32_t numTypeArg;
};

class DecompressPointerInstr : public ILInstr {
public:
	DecompressPointerInstr(AddrRange addrRange, VarStorage dst) : ILInstr(DecompressPointer, addrRange), dst(dst) {}
	DecompressPointerInstr() = delete;
	DecompressPointerInstr(DecompressPointerInstr&&) = delete;
	DecompressPointerInstr& operator=(const DecompressPointerInstr&) = delete;

	virtual std::string ToString() {
		return "DecompressPointer " + dst.Name();
	}

protected:
	VarStorage dst;
};

class SaveRegisterInstr : public ILInstr {
public:
	SaveRegisterInstr(AddrRange addrRange, A64::Register srcReg) : ILInstr(SaveRegister, addrRange), srcReg(srcReg) {}
	SaveRegisterInstr() = delete;
	SaveRegisterInstr(SaveRegisterInstr&&) = delete;
	SaveRegisterInstr& operator=(const SaveRegisterInstr&) = delete;

	virtual std::string ToString() {
		return std::string("SaveReg ") + srcReg.Name();
	}

protected:
	A64::Register srcReg;
};

class RestoreRegisterInstr : public ILInstr {
public:
	RestoreRegisterInstr(AddrRange addrRange, A64::Register dstReg) : ILInstr(RestoreRegister, addrRange), dstReg(dstReg) {}
	RestoreRegisterInstr() = delete;
	RestoreRegisterInstr(RestoreRegisterInstr&&) = delete;
	RestoreRegisterInstr& operator=(const RestoreRegisterInstr&) = delete;

	virtual std::string ToString() {
		return std::string("RestoreReg ") + dstReg.Name();
	}

protected:
	A64::Register dstReg;
};

class SetupParametersInstr : public ILInstr {
public:
	SetupParametersInstr(AddrRange addrRange, FnParams* params) : ILInstr(SetupParameters, addrRange), params(params) {}
	SetupParametersInstr() = delete;
	SetupParametersInstr(SetupParametersInstr&&) = delete;
	SetupParametersInstr& operator=(const SetupParametersInstr&) = delete;

	virtual std::string ToString();

	FnParams* params;
};

class InitAsyncInstr : public ILInstr {
public:
	InitAsyncInstr(AddrRange addrRange, DartType* retType) : ILInstr(InitAsync, addrRange), retType(retType) {}
	InitAsyncInstr() = delete;
	InitAsyncInstr(InitAsyncInstr&&) = delete;
	InitAsyncInstr& operator=(const InitAsyncInstr&) = delete;

	virtual std::string ToString() {
		return "InitAsync() -> " + retType->ToString();
	}

protected:
	DartType* retType;
};

class GdtCallInstr : public ILInstr {
public:
	GdtCallInstr(AddrRange addrRange, int64_t offset) : ILInstr(GdtCall, addrRange), offset(offset) {}
	GdtCallInstr() = delete;
	GdtCallInstr(GdtCallInstr&&) = delete;
	GdtCallInstr& operator=(const GdtCallInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("r0 = GDT[cid_x0 + {:#x}]()", offset);
	}

protected:
	int64_t offset;
};

class CallInstr : public ILInstr {
public:
	CallInstr(AddrRange addrRange, DartFnBase* fnBase, uint64_t addr) : ILInstr(Call, addrRange), fnBase(fnBase), addr(addr) {}
	CallInstr() = delete;
	CallInstr(CallInstr&&) = delete;
	CallInstr& operator=(const CallInstr&) = delete;

	virtual std::string ToString() {
		if (fnBase != nullptr)
			return fmt::format("r0 = {}()", fnBase->Name());
		return fmt::format("r0 = call {:#x}", addr);
	}

	DartFnBase* GetFunction() {
		return fnBase;
	}

	uint64_t GetCallAddress() {
		return addr;
	}

protected:
	DartFnBase* fnBase;
	uint64_t addr;
};

class ReturnInstr : public ILInstr {
public:
	ReturnInstr(AddrRange addrRange) : ILInstr(Return, addrRange) {}
	ReturnInstr() = delete;
	ReturnInstr(ReturnInstr&&) = delete;
	ReturnInstr& operator=(const ReturnInstr&) = delete;

	virtual std::string ToString() {
		return "ret";
	}
};

class BranchIfSmiInstr : public ILInstr {
public:
	BranchIfSmiInstr(AddrRange addrRange, A64::Register objReg, int64_t branchAddr) : ILInstr(BranchIfSmi, addrRange), objReg(objReg), branchAddr(branchAddr) {}
	BranchIfSmiInstr() = delete;
	BranchIfSmiInstr(BranchIfSmiInstr&&) = delete;
	BranchIfSmiInstr& operator=(const BranchIfSmiInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("branchIfSmi({}, {:#x})", objReg.Name(), branchAddr);
	}

	A64::Register objReg;
	int64_t branchAddr;
};

class LoadClassIdInstr : public ILInstr {
public:
	LoadClassIdInstr(AddrRange addrRange, A64::Register objReg, A64::Register cidReg) : ILInstr(LoadClassId, addrRange), objReg(objReg), cidReg(cidReg) {}
	LoadClassIdInstr() = delete;
	LoadClassIdInstr(LoadClassIdInstr&&) = delete;
	LoadClassIdInstr& operator=(const LoadClassIdInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = LoadClassIdInstr({})", cidReg.Name(), objReg.Name());
	}

	A64::Register objReg;
	A64::Register cidReg;
};

class LoadTaggedClassIdMayBeSmiInstr : public ILInstr {
public:
	LoadTaggedClassIdMayBeSmiInstr(AddrRange addrRange, std::unique_ptr<LoadValueInstr> il_loadImm,
		std::unique_ptr<BranchIfSmiInstr> il_branchIfSmi, std::unique_ptr<LoadClassIdInstr> il_loadClassId) 
		: ILInstr(LoadTaggedClassIdMayBeSmi, addrRange), taggedCidReg(il_loadClassId->cidReg), objReg(il_loadClassId->objReg),
		il_loadImm(std::move(il_loadImm)), il_branchIfSmi(std::move(il_branchIfSmi)), il_loadClassId(std::move(il_loadClassId)) {}
	LoadTaggedClassIdMayBeSmiInstr() = delete;
	LoadTaggedClassIdMayBeSmiInstr(LoadTaggedClassIdMayBeSmiInstr&&) = delete;
	LoadTaggedClassIdMayBeSmiInstr& operator=(const LoadTaggedClassIdMayBeSmiInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = LoadTaggedClassIdMayBeSmiInstr({})", taggedCidReg.Name(), objReg.Name());
	}

	A64::Register taggedCidReg;
	A64::Register objReg;
	std::unique_ptr<LoadValueInstr> il_loadImm;
	std::unique_ptr<BranchIfSmiInstr> il_branchIfSmi;
	std::unique_ptr<LoadClassIdInstr> il_loadClassId;
};

class BoxInt64Instr : public ILInstr {
public:
	BoxInt64Instr(AddrRange addrRange, A64::Register objReg, A64::Register srcReg) : ILInstr(BoxInt64, addrRange), objReg(objReg), srcReg(srcReg) {}
	BoxInt64Instr() = delete;
	BoxInt64Instr(BoxInt64Instr&&) = delete;
	BoxInt64Instr& operator=(const BoxInt64Instr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = BoxInt64Instr({})", objReg.Name(), srcReg.Name());
	}

	A64::Register objReg;
	A64::Register srcReg;
};

class LoadInt32Instr : public ILInstr {
public:
	LoadInt32Instr(AddrRange addrRange, A64::Register dstReg, A64::Register srcObjReg) : ILInstr(LoadInt32, addrRange), dstReg(dstReg), srcObjReg(srcObjReg) {}
	LoadInt32Instr() = delete;
	LoadInt32Instr(LoadInt32Instr&&) = delete;
	LoadInt32Instr& operator=(const LoadInt32Instr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = LoadInt32Instr({})", dstReg.Name(), srcObjReg.Name());
	}

	A64::Register dstReg;
	A64::Register srcObjReg;
};

class AllocateObjectInstr : public ILInstr {
public:
	AllocateObjectInstr(AddrRange addrRange, A64::Register dstReg, DartClass& dartCls)
		: ILInstr(AllocateObject, addrRange), dstReg(dstReg), dartCls(dartCls){}
	AllocateObjectInstr() = delete;
	AllocateObjectInstr(AllocateObjectInstr&&) = delete;
	AllocateObjectInstr& operator=(const AllocateObjectInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = inline_Allocate{}()", dstReg.Name(), dartCls.Name());
	}

	A64::Register dstReg;
	DartClass& dartCls;
};

struct ArrayOp {
	enum ArrayType {
		List,
		TypedUnknown,
		TypedSigned,
		TypedUnsigned,
		Unknown, // might be Object, List, or TypedUnknown
	};
	uint8_t size;
	bool isLoad; // else isStore
	ArrayType arrType;

	ArrayOp() : size(0), isLoad(false), arrType(Unknown) {}
	ArrayOp(uint8_t size, bool isLoad, ArrayType arrType) : size(size), isLoad(isLoad), arrType(arrType) {}

	bool IsArrayOp() const { return size != 0; }
	uint8_t SizeLog2() const {
		if (size == 8) return 3;
		if (size == 4) return 2;
		if (size == 2) return 1;
		if (size == 1) return 0;
		return 255;
	}
	// TODO: correct List type or typed_data type (Int32x4, ...)
	std::string ToString() {
		switch (arrType) {
		case List: return fmt::format("List_{}", size);
		case TypedUnknown: return fmt::format("TypeUnknown_{}", size);
		case TypedSigned: return fmt::format("TypedSigned_{}", size);
		case TypedUnsigned: return fmt::format("TypedUnsigned_{}", size);
		case Unknown: return fmt::format("Unknown_{}", size);
		default: return "";
		}
	}
};

class LoadArrayElementInstr : public ILInstr {
public:
	LoadArrayElementInstr(AddrRange addrRange, A64::Register dstReg, A64::Register arrReg, VarStorage idx, ArrayOp arrayOp)
		: ILInstr(LoadArrayElement, addrRange), dstReg(dstReg), arrReg(arrReg), idx(idx), arrayOp(arrayOp) {}
	LoadArrayElementInstr() = delete;
	LoadArrayElementInstr(LoadArrayElementInstr&&) = delete;
	LoadArrayElementInstr& operator=(const LoadArrayElementInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("ArrayLoad: {} = {}[{}]  ; {}", dstReg.Name(), arrReg.Name(), idx.Name(), arrayOp.ToString());
	}

	A64::Register dstReg;
	A64::Register arrReg;
	VarStorage idx;
	ArrayOp arrayOp;
};

class StoreArrayElementInstr : public ILInstr {
public:
	StoreArrayElementInstr(AddrRange addrRange, A64::Register valReg, A64::Register arrReg, VarStorage idx, ArrayOp arrayOp)
		: ILInstr(StoreArrayElement, addrRange), valReg(valReg), arrReg(arrReg), idx(idx), arrayOp(arrayOp) {}
	StoreArrayElementInstr() = delete;
	StoreArrayElementInstr(StoreArrayElementInstr&&) = delete;
	StoreArrayElementInstr& operator=(const StoreArrayElementInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("ArrayStore: {}[{}] = {}  ; {}", arrReg.Name(), idx.Name(), valReg.Name(), arrayOp.ToString());
	}

	A64::Register valReg;
	A64::Register arrReg;
	VarStorage idx;
	ArrayOp arrayOp;
};

class LoadFieldInstr : public ILInstr {
public:
	LoadFieldInstr(AddrRange addrRange, A64::Register dstReg, A64::Register objReg, uint32_t offset)
		: ILInstr(LoadField, addrRange), dstReg(dstReg), objReg(objReg), offset(offset) {}
	LoadFieldInstr() = delete;
	LoadFieldInstr(LoadFieldInstr&&) = delete;
	LoadFieldInstr& operator=(const LoadFieldInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("LoadField: {} = {}->field_{:x}", dstReg.Name(), objReg.Name(), offset);
	}

	A64::Register dstReg;
	A64::Register objReg;
	uint32_t offset;
};

class StoreFieldInstr : public ILInstr {
public:
	StoreFieldInstr(AddrRange addrRange, A64::Register valReg, A64::Register objReg, uint32_t offset)
		: ILInstr(StoreField, addrRange), valReg(valReg), objReg(objReg), offset(offset) {}
	StoreFieldInstr(cs_insn* insn, A64::Register valReg, A64::Register objReg, uint32_t offset)
		: ILInstr(StoreField, insn), valReg(valReg), objReg(objReg), offset(offset) {}
	StoreFieldInstr() = delete;
	StoreFieldInstr(StoreFieldInstr&&) = delete;
	StoreFieldInstr& operator=(const StoreFieldInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("StoreField: {}->field_{:x} = {}", objReg.Name(), offset, valReg.Name());
	}

	A64::Register valReg;
	A64::Register objReg;
	uint32_t offset;
};

class InitLateStaticFieldInstr : public ILInstr {
public:
	// TODO: add pool object offset or pointer
	InitLateStaticFieldInstr(AddrRange addrRange, VarStorage dst, DartField& field)
		: ILInstr(InitLateStaticField, addrRange), dst(dst), field(field) {}
	InitLateStaticFieldInstr() = delete;
	InitLateStaticFieldInstr(InitLateStaticFieldInstr&&) = delete;
	InitLateStaticFieldInstr& operator=(const InitLateStaticFieldInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = InitLateStaticField({:#x}) // {}", dst.Name(), field.Offset(), field.FullName());
	}

	std::string ValueExpression() {
		return field.Name();
	}

protected:
	VarStorage dst;
	DartField& field;
};

class LoadStaticFieldInstr : public ILInstr {
public:
	LoadStaticFieldInstr(AddrRange addrRange, A64::Register dstReg, uint32_t fieldOffset)
		: ILInstr(LoadStaticField, addrRange), dstReg(dstReg), fieldOffset(fieldOffset) {}
	LoadStaticFieldInstr() = delete;
	LoadStaticFieldInstr(LoadStaticFieldInstr&&) = delete;
	LoadStaticFieldInstr& operator=(const LoadStaticFieldInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} = LoadStaticField({:#x})", dstReg.Name(), fieldOffset);
	}

protected:
	A64::Register dstReg;
	uint32_t fieldOffset;
};

class StoreStaticFieldInstr : public ILInstr {
public:
	StoreStaticFieldInstr(AddrRange addrRange, A64::Register valReg, uint32_t fieldOffset)
		: ILInstr(LoadStaticField, addrRange), valReg(valReg), fieldOffset(fieldOffset) {}
	StoreStaticFieldInstr() = delete;
	StoreStaticFieldInstr(StoreStaticFieldInstr&&) = delete;
	StoreStaticFieldInstr& operator=(const StoreStaticFieldInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("StoreStaticField({:#x}, {})", fieldOffset, valReg.Name());
	}

protected:
	A64::Register valReg;
	uint32_t fieldOffset;
};

class WriteBarrierInstr : public ILInstr {
public:
	WriteBarrierInstr(AddrRange addrRange, A64::Register objReg, A64::Register valReg, bool isArray)
		: ILInstr(WriteBarrier, addrRange), objReg(objReg), valReg(valReg), isArray(isArray) {}
	WriteBarrierInstr() = delete;
	WriteBarrierInstr(WriteBarrierInstr&&) = delete;
	WriteBarrierInstr& operator=(const WriteBarrierInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{}WriteBarrierInstr(obj = {}, val = {})", isArray ? "Array" : "", objReg.Name(), valReg.Name());
	}

	A64::Register objReg;
	A64::Register valReg;
	bool isArray;
};

class TestTypeInstr : public ILInstr {
public:
	TestTypeInstr(AddrRange addrRange, A64::Register srcReg, std::string typeName)
		: ILInstr(TestType, addrRange), srcReg(srcReg), typeName(std::move(typeName)) {}
	TestTypeInstr() = delete;
	TestTypeInstr(TestTypeInstr&&) = delete;
	TestTypeInstr& operator=(const TestTypeInstr&) = delete;

	virtual std::string ToString() {
		return fmt::format("{} as {}", srcReg.Name(), typeName);
	}

	A64::Register srcReg;
	std::string typeName;
};
