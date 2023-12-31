#!/usr/bin/env python3

# Script to read x86reference.xml and convert it into C data structures

import operator
import sys
from dataclasses import dataclass
from enum import Enum
from pprint import pprint
from typing import List, Optional, Set, Tuple

import bs4
import jinja2
from jinja2 import Template


class Size(Enum):
    AUTO = 0
    SIZE08 = "SIZE08"
    SIZE16 = "SIZE16"
    SIZE32 = "SIZE32"
    SIZE64 = "SIZE64"

    def __lt__(self, other):
        return operator.lt(self.value, other.value)


@dataclass
class LongOpCode:
    mnem: str
    size: Optional[Size] = None


class UnsupportedOperandError(Exception):
    pass


def make_sized_aliases(mnem: str, sizes=None, short_mnem=None, xmm=False):
    if sizes is None:
        if xmm:
            sizes = {Size.SIZE16, Size.SIZE32}
        else:
            sizes = {Size.SIZE08, Size.SIZE16, Size.SIZE32, Size.SIZE64}

    if xmm:
        # FP registers

        result = {}

        if Size.SIZE16 in sizes:
            result[f"{mnem}s"] = LongOpCode(mnem=f"{mnem}s", size=Size.SIZE16)
        if Size.SIZE32 in sizes:
            result[f"{mnem}d"] = LongOpCode(mnem=f"{mnem}d", size=Size.SIZE32)
    else:
        # Integer registers

        if short_mnem is None:
            short_mnem = mnem

        result = {mnem: LongOpCode(mnem=short_mnem, size=None)}

        if Size.SIZE08 in sizes:
            result[f"{mnem}b"] = LongOpCode(mnem=short_mnem, size=Size.SIZE08)
        if Size.SIZE16 in sizes:
            result[f"{mnem}w"] = LongOpCode(mnem=short_mnem, size=Size.SIZE16)
        if Size.SIZE32 in sizes:
            result[f"{mnem}l"] = LongOpCode(mnem=short_mnem, size=Size.SIZE32)
        if Size.SIZE64 in sizes:
            result[f"{mnem}q"] = LongOpCode(mnem=short_mnem, size=Size.SIZE64)

    return result


OPCODE_ALIASES = {
    **make_sized_aliases("div", sizes={Size.SIZE32, Size.SIZE64}),
    **make_sized_aliases("idiv", sizes={Size.SIZE32, Size.SIZE64}),
    **make_sized_aliases("lea", sizes={Size.SIZE64}),
    **make_sized_aliases("pop", sizes={Size.SIZE64}),
    **make_sized_aliases("push", sizes={Size.SIZE64}),
    **make_sized_aliases("ret", short_mnem="retn", sizes={Size.SIZE64}),
    "callq": LongOpCode(mnem="call"),
    "leaveq": LongOpCode(mnem="leave"),
    "movabsq": LongOpCode(mnem="mov", size=Size.SIZE64),
    "movsbw": LongOpCode(mnem="movsx"),
    "movsbl": LongOpCode(mnem="movsx"),
    "movsbq": LongOpCode(mnem="movsx"),
    "movslq": LongOpCode(mnem="movsxd"),
    "movswl": LongOpCode(mnem="movsx"),
    "movswq": LongOpCode(mnem="movsx"),
    "movzbl": LongOpCode(mnem="movzx"),
    "movzbq": LongOpCode(mnem="movzx"),
    "movzwl": LongOpCode(mnem="movzx"),
    "movzwq": LongOpCode(mnem="movzx"),
    "cwtd": LongOpCode(mnem="cwd", size=Size.SIZE16),
    "cltd": LongOpCode(mnem="cdq", size=Size.SIZE32),
    "cqto": LongOpCode(mnem="cqo", size=Size.SIZE64),
    "cvtsd2ss": LongOpCode(mnem="cvtsd2ss", size=Size.SIZE32),
    **make_sized_aliases("cvttss2si", {Size.SIZE32, Size.SIZE64}),
    **make_sized_aliases("cvttsd2si", {Size.SIZE32, Size.SIZE64}),
}

# Add all-sizes aliases
for mnem in (
    "add",
    "and",
    "cmp",
    "imul",
    "mov",
    "not",
    "or",
    "sar",
    "shl",
    "shr",
    "sub",
    "test",
    "xor",
):
    OPCODE_ALIASES.update(**make_sized_aliases(mnem))


# Aliases for 16, 32 and 64 bit integers
for mnem in (
    "cmovo",
    "cmovno",
    "cmovb",
    "cmovnae",
    "cmovc",
    "cmovnb",
    "cmovae",
    "cmovnc",
    "cmovz",
    "cmove",
    "cmovnz",
    "cmovne",
    "cmovbe",
    "cmovna",
    "cmovnbe",
    "cmova",
    "cmovs",
    "cmovns",
    "cmovp",
    "cmovpe",
    "cmovnp",
    "cmovpo",
    "cmovl",
    "cmovnge",
    "cmovnl",
    "cmovge",
    "cmovle",
    "cmovng",
    "cmovnle",
    "cmovg",
):
    OPCODE_ALIASES.update(
        **make_sized_aliases(mnem, sizes={Size.SIZE16, Size.SIZE32, Size.SIZE64})
    )

# Aliases for 16, 32 bit FP
# These aliases are needed, since in come cases, e.g. movss  (%rax), %xmm14
# where the size of the operation must be derived from the mnemonic. The XML
# doesn't have data to indicate the mnemonic size, other than the operand sizes.
for mnem in ("movs", "adds", "subs", "muls", "divs", "comis", "ucomis"):
    OPCODE_ALIASES.update(**make_sized_aliases(mnem, xmm=True))


class AddressingMode(Enum):
    C = "C"  # The reg field of the ModR/M byte selects a control register
    D = "D"  # The reg field of the ModR/M byte selects a debug register
    E = "E"  # The operand is either a general-purpose register or a memory address.
    EST = "EST"  # (Implies original E). A ModR/M byte follows the opcode and specifies the x87 FPU stack register.
    G = "G"  # The reg field of the ModR/M byte selects a general register
    I = "I"  # Immediate
    J = "J"  # RIP relative
    H = "H"  # The r/m field of the ModR/M byte always selects a general register, regardless of the mod field
    M = "M"  # The ModR/M byte may refer only to memory
    O = "O"  # Offset
    R = "R"  # Not used
    S = "S"  # Not used
    ST = "ST"  # x87 FPU stack register.
    T = "T"  # The reg field of the ModR/M byte selects a test register (only MOV (0F24, 0F26)).
    V = "V"  # The reg field of the ModR/M byte selects a 128-bit XMM register.
    W = "W"  # The operand is either a 128-bit XMM register or a memory address.
    Z = "Z"  # The three least-significant bits of the opcode byte selects a general-purpose register

    def to_c_enum(self):
        C_ENUMS = {
            "C": 1,
            "D": 2,
            "E": 3,
            "EST": 4,
            "G": 5,
            "I": 6,
            "J": 7,
            "H": 8,
            "M": 9,
            "O": 10,
            "R": 11,
            "S": 12,
            "ST": 13,
            "T": 14,
            "V": 15,
            "W": 16,
            "Z": 17,
        }

        return C_ENUMS[self.value]


class OperandType(Enum):
    b = "b"  # Byte
    bs = "bs"  # Byte, sign-extended to the size of the destination operand.
    bss = "bss"  # Byte, sign-extended to the size of the stack pointer (for example, PUSH (6A)).
    d = "d"  #  Doubleword
    dqp = "dqp"  # Doubleword, or quadword, promoted by REX.W in 64-bit mode
    q = "q"  # Quad
    ss = "ss"  #  Scalar element of a 128-bit packed single-precision floating data.
    sd = "sd"  #  Scalar element of a 128-bit packed double-precision floating data.
    v = "v"  #   Word or doubleword, depending on operand-size attribute (for example, INC (40), PUSH (50)).
    vds = "vds"  # Word or doubleword, depending on operand-size attribute, or doubleword, sign-extended to 64 bits for 64-bit operand size.
    vq = "vq"  # Quadword (default) or word if operand-size prefix is used (for example, PUSH (50)).
    vqp = "vqp"  # Word or doubleword, depending on operand-size attribute, or quadword, promoted by REX.W in 64-bit mode.
    vs = "vs"  # Word or doubleword sign extended to the size of the stack pointer (for example, PUSH (68)).
    w = "w"  #  Word


OPERAND_TYPE_TO_SIZES = {
    OperandType.b: set([Size.SIZE08]),
    OperandType.bs: set([Size.SIZE08]),
    OperandType.bss: set([Size.SIZE08]),
    OperandType.d: set([Size.SIZE32]),
    OperandType.dqp: set([Size.SIZE16, Size.SIZE32, Size.SIZE64]),
    OperandType.ss: set([Size.SIZE16]),
    OperandType.sd: set([Size.SIZE32]),
    OperandType.q: set([Size.SIZE64]),
    OperandType.v: set([Size.SIZE16, Size.SIZE32]),
    OperandType.vds: set([Size.SIZE16, Size.SIZE32]),
    OperandType.vq: set([Size.SIZE16, Size.SIZE64]),
    OperandType.vqp: set([Size.SIZE16, Size.SIZE32, Size.SIZE64]),
    OperandType.vs: set([Size.SIZE16, Size.SIZE32]),
    OperandType.w: set([Size.SIZE16]),
}

# Operand types that have the operand-size attribute set
OPERAND_TYPES_THAT_USE_OPERAND_SIZE = {
    OperandType.v,
    OperandType.vds,
    OperandType.vq,
    OperandType.vqp,
    OperandType.vs,
    OperandType.w,
}

SIGN_EXTENDED_OPERANDS = {
    OperandType.bs,
    OperandType.bss,
    OperandType.vds,
}

# Operands that have a 64-bit immediate when the opcode size is 64 bit
IMM64_OPERANDS = {
    OperandType.vqp,
}

WORD_OR_DOUBLE_WORD_OPERANDS = {
    OperandType.v,
    OperandType.vq,
}


@dataclass
class WasOperand:
    am: AddressingMode
    sizes: str
    uses_operand_size: int
    can_be_imm64: int
    sign_extended: int
    word_or_double_word_operand: int
    type: int

    @staticmethod
    def type_to_str(operand_type: OperandType):
        if not operand_type:
            return "0"

        sizes = OPERAND_TYPE_TO_SIZES[operand_type]
        if not len(sizes):
            return "0"

        return "|".join(s.value for s in sorted(sizes))

    @staticmethod
    def from_am_and_type(am, type):
        return WasOperand(
            am=am,
            sizes=WasOperand.type_to_str(type),
            uses_operand_size=int(type in OPERAND_TYPES_THAT_USE_OPERAND_SIZE),
            can_be_imm64=int(type in IMM64_OPERANDS),
            sign_extended=int(type in SIGN_EXTENDED_OPERANDS),
            word_or_double_word_operand=int(type in WORD_OR_DOUBLE_WORD_OPERANDS),
            type=type,
        )


@dataclass
class WasOpcode:
    mnem: str
    prefix: int  # prefix
    ohf_prefix: int  # 0x0f prefix
    value: int
    opcd_ext: int
    needs_mod_rm: int
    note: str
    op_size: int
    direction: int
    acc: int
    branch: int
    conver: int
    op1: WasOperand
    op2: WasOperand

    def __str__(self):
        opcd_ext = str(self.opcd_ext) if self.opcd_ext != -1 else " "
        if self.needs_mod_rm:
            opcd_ext = "r"

        direction = "dD"[self.direction] if self.direction != -1 else " "
        op_size = "wW"[self.op_size] if self.op_size != -1 else " "

        op1_amt = f"{self.op1.am.value if self.op1.am else ''}{self.op1.type.value if self.op1.type else ''}"
        op2_amt = f"{self.op2.am.value if self.op2.am else ''}{self.op2.type.value if self.op2.type else ''}"

        acc = "a" if self.acc else " "
        return (
            f"{self.prefix if self.prefix != '00' else '  '} "
            + f"{self.ohf_prefix if self.ohf_prefix != '00' else '  '} "
            + f"{self.value} "
            + direction
            + op_size
            + f"{'b' if self.branch else ' '}"
            + f"{'c' if self.conver else ' '} "
            + f"{opcd_ext:2s}"
            + f"{acc}  "
            + f"{self.mnem:10s}"
            + f"{op1_amt:5s} {op2_amt:5s}"
            + f"{self.note}"
        )


OPCODES = {opcode.mnem for opcode in OPCODE_ALIASES.values()}


def read_xml(input_path: str):
    with open(input_path, "r") as f:
        xml = f.read()

    data = bs4.BeautifulSoup(xml, features="xml")
    x86reference = data.find("x86reference")

    return x86reference


def parse_operand(entry, operand: str) -> List[WasOperand]:
    results = []

    for operand in entry.find_all(operand):
        if operand.get("displayed") == "no":
            continue

        try:
            am = AddressingMode(operand.a.text) if operand.a else None
            type = OperandType(operand.t.text) if operand.t else None
        except Exception as e:
            raise UnsupportedOperandError(f"{operand.a}{operand.t}")

        if type is None:
            type_from_attr = operand.get("type")
            if type_from_attr is not None:
                try:
                    type = OperandType(type_from_attr)
                except Exception as e:
                    raise UnsupportedOperandError(f"{am}{type_from_attr}")

        if operand.text == "ST":
            # This is not really an addressing mode, but it's convenient
            # to treat an ST src/dst as such
            am = AddressingMode.ST

        results.append(WasOperand.from_am_and_type(am, type))

    return results


def parse_operands(mnem, syntax) -> Tuple[WasOperand, WasOperand]:
    srcs = parse_operand(syntax, "src")
    dsts = parse_operand(syntax, "dst")

    op1 = WasOperand.from_am_and_type(None, None)
    op2 = WasOperand.from_am_and_type(None, None)

    if len(srcs) == 1 and len(dsts) == 0:
        op1 = srcs[0]
    elif len(srcs) == 0 and len(dsts) == 1:
        op1 = dsts[0]
    elif len(srcs) == 1 and len(dsts) == 1:
        op1 = srcs[0]
        op2 = dsts[0]
    elif len(srcs) == 2:
        # Don't ask. This makes the test mnemonic get encoded correctly
        op1 = srcs[1]
        op2 = srcs[0]
    elif len(srcs) == 0 and len(dsts) == 0:
        pass
    else:
        raise UnsupportedOperandError(
            f"Unable to interpret srcs/dsts for {mnem:10s} srcs={len(srcs)} dsts={len(dsts)}"
        )

    return op1, op2


def parse_pri_opcd(one_byte, ohf_prefix):
    was_opcodes = []

    for pri_opcd in one_byte.find_all("pri_opcd"):
        pri_opcd_was_opcodes = []
        found_invalid_flag = False

        value = pri_opcd["value"].lower()

        for entry in pri_opcd.find_all("entry"):
            pref = getattr(entry, "pref")
            if pref is not None:
                pref = pref.text.lower()
            else:
                pref = "00"

            note = entry.note.brief.text if entry.note else None

            attr = entry.get("attr")
            acc = attr == "acc"

            op_size = int(entry.get("op_size", -1))
            direction = int(entry.get("direction", -1))

            if note is not None and "Invalid Instruction in 64-Bit Mode" in note:
                found_invalid_flag = True
                continue

            opcd_ext = entry.opcd_ext.text if entry.opcd_ext else None

            r = entry.get("r") == "yes"

            branch = False
            conver = False

            for grp_name in ("grp1", "grp2", "grp3"):
                for grp in entry.find_all(grp_name):
                    if grp.text == "branch":
                        branch = True
                    elif grp.text == "conver":
                        conver = True

            for syntax in entry.find_all("syntax"):
                if syntax.mnem is None:
                    continue

                mnem = syntax.mnem.text.lower()
                if mnem not in OPCODE_ALIASES:
                    OPCODES.add(mnem)
                    OPCODE_ALIASES[mnem] = LongOpCode(mnem=mnem)

                try:
                    op1, op2 = parse_operands(mnem, syntax)
                except UnsupportedOperandError:
                    continue

                # Not implemented
                # The reg field of the ModR/M byte selects a control register (only MOV (0F20, 0F22)).
                if op2.am == AddressingMode.C or op1.am == AddressingMode.C:
                    continue

                # Not implemented
                # The reg field of the ModR/M byte selects a debug register (only MOV (0F21, 0F23)).
                if op2.am == AddressingMode.D or op1.am == AddressingMode.D:
                    continue

                # 16-bit segment registers not used in long mode
                if op2.am == AddressingMode.S or op1.am == AddressingMode.S:
                    continue

                # Replicate bug in GNU as reversed vs. non-reversed mnemonics for x87
                # Non-commutative floating point instructions with register operands
                # (like fdiv vs. fdivr) is the wrong way round.
                # https://stackoverflow.com/questions/56210264/objdump-swapping-fsubrp-to-fsubp-on-compiled-assembly
                # https://sourceware.org/binutils/docs/as/i386_002dBugs.html
                if value == "de":  # Among others, subp, subrp, divp, divrp
                    BUG_MAP = {
                        "4": "5",
                        "5": "4",
                        "6": "7",
                        "7": "6",
                    }
                    if opcd_ext in BUG_MAP:
                        opcd_ext = BUG_MAP[opcd_ext]

                was_opcode = WasOpcode(
                    mnem=mnem,
                    prefix=pref,
                    ohf_prefix=ohf_prefix,
                    value=value,
                    opcd_ext=opcd_ext if opcd_ext is not None else -1,
                    needs_mod_rm=1 if r else 0,
                    note=note,
                    op_size=op_size,
                    direction=direction,
                    acc=int(acc),
                    branch=int(branch),
                    conver=int(conver),
                    op1=op1,
                    op2=op2,
                )

                print(was_opcode)

                pri_opcd_was_opcodes.append(was_opcode)

        if not found_invalid_flag:
            was_opcodes += pri_opcd_was_opcodes

    return was_opcodes


def make_was_opcodes(x86reference):
    was_opcodes = []

    for one_byte in x86reference.find_all("one-byte"):
        was_opcodes += parse_pri_opcd(one_byte, "00")

    for two_byte in x86reference.find_all("two-byte"):
        was_opcodes += parse_pri_opcd(two_byte, "0f")

    return was_opcodes


def output_code(was_opcodes: List[WasOpcode], output_path: str):
    template = Template(open("scripts/opcodes.j2").read())

    with open(output_path, "w") as f:
        f.write(
            template.render(
                generator=sys.argv[0],
                opcodes=was_opcodes,
                opcode_aliases=OPCODE_ALIASES,
            )
        )


if __name__ == "__main__":
    input_path = sys.argv[1]
    output_path = sys.argv[2]

    x86reference = read_xml(input_path)
    was_opcodes = make_was_opcodes(x86reference)
    output_code(was_opcodes, output_path)
