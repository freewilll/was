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
    SIZEST = "SIZEST"

    def __lt__(self, other):
        return operator.lt(self.value, other.value)

    def to_short(self):
        return self.value[4:6]


@dataclass
class LongOpCode:
    mnem: str
    op1_size: Optional[Size] = None
    op2_size: Optional[Size] = None
    op3_size: Optional[Size] = None


class UnsupportedOperandError(Exception):
    pass


def make_long_opcode(mnem, size):
    return LongOpCode(mnem=mnem, op1_size=size, op2_size=size, op3_size=size)


def make_long_opcode2(mnem, op1_size, op2_size):
    return LongOpCode(mnem=mnem, op1_size=op1_size, op2_size=op2_size, op3_size=None)


def make_sized_aliases(mnem: str, sizes=None, short_mnem=None, xmm=False):
    if sizes is None:
        if xmm:
            sizes = {Size.SIZE16, Size.SIZE32}
        else:
            sizes = {Size.SIZE08, Size.SIZE16, Size.SIZE32, Size.SIZE64}

    if xmm:
        # FP registers

        result = []

        if Size.SIZE16 in sizes:
            result.append((f"{mnem}s", make_long_opcode(f"{mnem}s", Size.SIZE16)))
        if Size.SIZE32 in sizes:
            result.append((f"{mnem}d", make_long_opcode(f"{mnem}d", Size.SIZE32)))
    else:
        # Integer registers

        if short_mnem is None:
            short_mnem = mnem

        result = [(mnem, make_long_opcode(short_mnem, None))]

        if Size.SIZE08 in sizes:
            result.append((f"{mnem}b", make_long_opcode(short_mnem, Size.SIZE08)))
        if Size.SIZE16 in sizes:
            result.append((f"{mnem}w", make_long_opcode(short_mnem, Size.SIZE16)))
        if Size.SIZE32 in sizes:
            result.append((f"{mnem}l", make_long_opcode(short_mnem, Size.SIZE32)))
        if Size.SIZE64 in sizes:
            result.append((f"{mnem}q", make_long_opcode(short_mnem, Size.SIZE64)))

    return result


OPCODE_ALIASES = (
    (
        make_sized_aliases("div", sizes={Size.SIZE32, Size.SIZE64})
        + make_sized_aliases("idiv", sizes={Size.SIZE32, Size.SIZE64})
        + make_sized_aliases("lea", sizes={Size.SIZE64})
        + make_sized_aliases("pop", sizes={Size.SIZE64})
        + make_sized_aliases("push", sizes={Size.SIZE64})
        + make_sized_aliases("ret", short_mnem="retn", sizes={Size.SIZE64})
        + make_sized_aliases("cvttss2si", {Size.SIZE32, Size.SIZE64})
        + make_sized_aliases("cvttsd2si", {Size.SIZE32, Size.SIZE64})
    )
    + [
        ("callq", LongOpCode(mnem="call")),
        ("leaveq", LongOpCode(mnem="leave")),
        ("movabsq", make_long_opcode("mov", Size.SIZE64)),
        ("movsbw", make_long_opcode2("movsx", Size.SIZE08, Size.SIZE32)),
        ("movsbl", make_long_opcode2("movsx", Size.SIZE08, Size.SIZE32)),
        ("movsbq", make_long_opcode2("movsx", Size.SIZE08, Size.SIZE64)),
        ("movswl", make_long_opcode2("movsx", Size.SIZE16, Size.SIZE32)),
        ("movswq", make_long_opcode2("movsx", Size.SIZE16, Size.SIZE64)),
        ("movslq", make_long_opcode2("movsxd", Size.SIZE32, Size.SIZE64)),
        ("movzbl", make_long_opcode2("movzx", Size.SIZE08, Size.SIZE32)),
        ("movzbw", make_long_opcode2("movzx", Size.SIZE08, Size.SIZE16)),
        ("movzbq", make_long_opcode2("movzx", Size.SIZE08, Size.SIZE64)),
        ("movzwl", make_long_opcode2("movzx", Size.SIZE16, Size.SIZE32)),
        ("movzwq", make_long_opcode2("movzx", Size.SIZE16, Size.SIZE64)),
        ("cwtd", make_long_opcode2("cwd", Size.SIZE16, Size.SIZE32)),
        ("cltd", make_long_opcode2("cdq", Size.SIZE32, Size.SIZE32)),
        ("cqto", make_long_opcode2("cqo", Size.SIZE64, Size.SIZE64)),
        ("cvtsd2ss", make_long_opcode2("cvtsd2ss", Size.SIZE32, Size.SIZE16)),
        ("cvtsi2ssl", make_long_opcode2("cvtsi2ss", Size.SIZE32, Size.SIZE16)),
        ("cvtsi2ssq", make_long_opcode2("cvtsi2ss", Size.SIZE32, Size.SIZE32)),
        ("cvtsi2sdl", make_long_opcode2("cvtsi2sd", Size.SIZE32, Size.SIZE32)),
        ("cvtsi2sdq", make_long_opcode2("cvtsi2sd", Size.SIZE64, Size.SIZE32)),
        ("movq", make_long_opcode("movq", Size.SIZE64)),  # reg <-> xmm conversion
    ]
) + [
    ("fild", make_long_opcode("fild", Size.SIZE16)),
    ("filds", make_long_opcode("fild", Size.SIZE16)),
    ("fildl", make_long_opcode("fild", Size.SIZE32)),
    ("fildq", make_long_opcode("fild", Size.SIZE64)),
    ("fildll", make_long_opcode("fild", Size.SIZE64)),
    ("fistp", make_long_opcode("fistp", Size.SIZE16)),
    ("fistps", make_long_opcode("fistp", Size.SIZE16)),
    ("fistpl", make_long_opcode("fistp", Size.SIZE32)),
    ("fistpq", make_long_opcode("fistp", Size.SIZE64)),
    ("fistpll", make_long_opcode("fistp", Size.SIZE64)),
    ("fadds", make_long_opcode("fadd", Size.SIZE32)),
    ("fld", make_long_opcode("fld", Size.SIZE32)),
    ("flds", make_long_opcode("fld", Size.SIZE32)),
    ("fldt", make_long_opcode("fld", Size.SIZEST)),
    ("fldl", make_long_opcode("fld", Size.SIZE64)),
    ("fstps", make_long_opcode("fstp", Size.SIZE32)),
    ("fstpt", make_long_opcode("fstp", Size.SIZEST)),
    ("fstpl", make_long_opcode("fstp", Size.SIZE64)),
]


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
    OPCODE_ALIASES += make_sized_aliases(mnem)


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
    OPCODE_ALIASES += make_sized_aliases(
        mnem, sizes={Size.SIZE16, Size.SIZE32, Size.SIZE64}
    )


# Aliases for 16, 32 bit FP
# These aliases are needed, since in come cases, e.g. movss  (%rax), %xmm14
# where the size of the operation must be derived from the mnemonic. The XML
# doesn't have data to indicate the mnemonic size, other than the operand sizes.
for mnem in ("movs", "adds", "subs", "muls", "divs", "comis", "ucomis"):
    OPCODE_ALIASES += make_sized_aliases(mnem, xmm=True)


class AddressingMode(Enum):
    C = "C"  # The reg field of the ModR/M byte selects a control register
    D = "D"  # The reg field of the ModR/M byte selects a debug register
    E = "E"  # The operand is either a general-purpose register or a memory address.
    ES = "ES"  # (Implies original E).  The operand is either a x87 FPU stack register or a memory address.
    EST = "EST"  # (Implies original E). A ModR/M byte follows the opcode and specifies the x87 FPU stack register.
    G = "G"  # The reg field of the ModR/M byte selects a general register
    I = "I"  # Immediate
    J = "J"  # RIP relative
    H = "H"  # The r/m field of the ModR/M byte always selects a general register, regardless of the mod field
    M = "M"  # The ModR/M byte may refer only to memory
    S = "S"  # Not used
    ST = "ST"  # x87 FPU stack register.
    V = "V"  # The reg field of the ModR/M byte selects a 128-bit XMM register.
    W = "W"  # The operand is either a 128-bit XMM register or a memory address.
    Z = "Z"  # The three least-significant bits of the opcode byte selects a general-purpose register


class OperandType(Enum):
    b = "b"  # Byte
    bs = "bs"  # Byte, sign-extended to the size of the destination operand.
    bss = "bss"  # Byte, sign-extended to the size of the stack pointer (for example, PUSH (6A)).
    d = "d"  #  Doubleword
    di = "di"  #  Doubleword Integer (x87 FPU only)
    dr = "dr"  #  Double-real. Only x87 FPU instructions
    dq = "dq"  # Double-quadword, regardless of operand-size attribute
    dqp = "dqp"  # Double-quadword, regardless of operand-size attribute (for example, CMPXCHG16B).
    er = "er"  # Extended-real. Only x87 FPU instructions).
    q = "q"  # Quad
    qi = "qi"  # Quad Integer (x87 FPU only)
    qp = "qp"  # Quadword, promoted by REX.W (for example, IRETQ).
    sr = "sr"  # Single-real (x87 FPU only)
    ss = "ss"  #  Scalar element of a 128-bit packed single-precision floating data.
    sd = "sd"  #  Scalar element of a 128-bit packed double-precision floating data.
    v = "v"  #   Word or doubleword, depending on operand-size attribute (for example, INC (40), PUSH (50)).
    vds = "vds"  # Word or doubleword, depending on operand-size attribute, or doubleword, sign-extended to 64 bits for 64-bit operand size.
    vq = "vq"  # Quadword (default) or word if operand-size prefix is used (for example, PUSH (50)).
    vqp = "vqp"  # Word or doubleword, depending on operand-size attribute, or quadword, promoted by REX.W in 64-bit mode.
    vs = "vs"  # Word or doubleword sign extended to the size of the stack pointer (for example, PUSH (68)).
    w = "w"  #  Word
    wi = "wi"  #  Word Integer (x87 FPU only)
    one = "1"  # Immediate 1. Not an official operand type, but convenient.


OPERAND_TYPE_TO_SIZES = {
    OperandType.b: set([Size.SIZE08]),
    OperandType.bs: set([Size.SIZE08]),
    OperandType.bss: set([Size.SIZE08]),
    OperandType.d: set([Size.SIZE32]),
    OperandType.di: set([Size.SIZE32]),
    OperandType.dq: set([Size.SIZE64]),
    OperandType.dqp: set([Size.SIZE32, Size.SIZE64]),
    OperandType.dr: set([Size.SIZE64]),
    OperandType.er: set([Size.SIZEST]),
    OperandType.sr: set([Size.SIZE32]),
    OperandType.ss: set([Size.SIZE16]),
    OperandType.sd: set([Size.SIZE32]),
    OperandType.q: set([Size.SIZE64]),
    OperandType.qi: set([Size.SIZE64]),
    OperandType.qp: set([Size.SIZE64]),
    OperandType.v: set([Size.SIZE16, Size.SIZE32]),
    OperandType.vds: set([Size.SIZE16, Size.SIZE32]),
    OperandType.vq: set([Size.SIZE16, Size.SIZE64]),
    OperandType.vqp: set([Size.SIZE16, Size.SIZE32, Size.SIZE64]),
    OperandType.vs: set([Size.SIZE16, Size.SIZE32]),
    OperandType.w: set([Size.SIZE16]),
    OperandType.wi: set([Size.SIZE16]),
    OperandType.one: set([Size.SIZE08]),
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
    word_or_double_word_operand: int
    type: int
    is_gen_reg: bool = False
    gen_reg_nr: int = 0  # General register number. Only usable if is_is_gen_reg is True

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
            word_or_double_word_operand=int(type in WORD_OR_DOUBLE_WORD_OPERANDS),
            type=type,
        )


@dataclass
class WasOpcode:
    mnem: str
    prefix: int  # prefix
    ohf_prefix: int  # 0x0f prefix
    value: int
    sec_opcd: int
    opcd_ext: int
    needs_mod_rm: int
    note: str
    op_size: int
    direction: int
    acc: int
    branch: int
    conver: int
    x87fpu: int
    op1: WasOperand
    op2: WasOperand
    op3: WasOperand

    def __str__(self):
        opcd_ext = str(self.opcd_ext) if self.opcd_ext != -1 else " "
        if self.needs_mod_rm:
            opcd_ext = "r"

        direction = "dD"[self.direction] if self.direction != -1 else " "
        op_size = "wW"[self.op_size] if self.op_size != -1 else " "

        op1_amt = f"{self.op1.am.value if self.op1.am else ''}{self.op1.type.value if self.op1.type else ''}"
        op2_amt = f"{self.op2.am.value if self.op2.am else ''}{self.op2.type.value if self.op2.type else ''}"
        op3_amt = f"{self.op3.am.value if self.op3.am else ''}{self.op3.type.value if self.op3.type else ''}"

        # General registers
        if self.op1.is_gen_reg:
            op1_amt = f"ge{self.op1.gen_reg_nr}{op1_amt}"
        if self.op2.is_gen_reg:
            op2_amt = f"ge{self.op2.gen_reg_nr}{op2_amt}"
        if self.op3.is_gen_reg:
            op3_amt = f"ge{self.op3.gen_reg_nr}{op3_amt}"

        acc = "a" if self.acc else " "
        return (
            f"{self.prefix if self.prefix != '00' else '  '} "
            + f"{self.ohf_prefix if self.ohf_prefix != '00' else '  '} "
            + f"{self.value} "
            + f"{self.sec_opcd if self.sec_opcd is not None else '  '} "
            + direction
            + op_size
            + f"{'b' if self.branch else ' '}"
            + f"{'c' if self.conver else ' '} "
            + f"{'x' if self.x87fpu else ' '} "
            + f"{opcd_ext:2s}"
            + f"{acc}  "
            + f"{self.mnem:10s}"
            + f"{op1_amt:5s} {op2_amt:5s} {op3_amt:5s}"
            + f"{self.note}"
        )


OPCODES = {opcode_alias_tuple[1].mnem for opcode_alias_tuple in OPCODE_ALIASES}


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

        # Immediate 1
        address = operand.get("address")
        if address is not None:
            if address == "I":
                am = AddressingMode.I
                type = OperandType.one

        was_operand = WasOperand.from_am_and_type(am, type)
        results.append(was_operand)

        group = operand.get("group")
        displayed = operand.get("displayed")
        if group is not None and group == "gen" and displayed is None:
            nr = operand.get("nr")
            type_ = operand.get("type")
            size = {
                # Bizarrely, in practice v and vqp are respectively 32 and 64 bit registers.
                # Not sure why l and q could not have been used for those.
                "b": Size.SIZE08,
                "w": Size.SIZE16,
                "l": Size.SIZE32,
                "q": Size.SIZE64,
                "v": Size.SIZE32,
                "vqp": Size.SIZE64,
            }[type_]

            if size is not None:
                was_operand.is_gen_reg = True
                was_operand.gen_reg_nr = nr

    return results


def parse_operands(mnem, syntax) -> Tuple[WasOperand, WasOperand]:
    # The operands are backwards in the XML from the point of view of the GAS
    # syntax so need to be reversed.

    srcs = parse_operand(syntax, "src")
    dsts = parse_operand(syntax, "dst")

    op1 = WasOperand.from_am_and_type(None, None)
    op2 = WasOperand.from_am_and_type(None, None)
    op3 = WasOperand.from_am_and_type(None, None)

    if len(srcs) == 1 and len(dsts) == 0:
        op1 = srcs[0]
    elif len(srcs) == 0 and len(dsts) == 1:
        op1 = dsts[0]
    elif len(srcs) == 1 and len(dsts) == 1:
        op1 = srcs[0]
        op2 = dsts[0]
    elif len(srcs) == 2 and len(dsts) == 0:
        op1 = srcs[1]
        op2 = srcs[0]
    elif len(srcs) == 2 and len(dsts) == 1:
        op1 = srcs[1]
        op2 = srcs[0]
        op3 = dsts[0]
    elif len(srcs) == 0 and len(dsts) == 0:
        pass
    else:
        raise UnsupportedOperandError(
            f"Unable to interpret srcs/dsts for {mnem:10s} srcs={len(srcs)} dsts={len(dsts)}"
        )

    return op1, op2, op3


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
            sec_opcd = entry.sec_opcd.text if entry.sec_opcd else None

            # if sec_opcd is set, opcd_ext is ignored
            if sec_opcd is not None:
                sec_opcd = sec_opcd.lower()
                opcd_ext = None

            r = entry.get("r") == "yes"

            branch = False
            conver = False
            x87fpu = False

            for grp_name in ("grp1", "grp2", "grp3"):
                for grp in entry.find_all(grp_name):
                    if grp.text == "branch":
                        branch = True
                    elif grp.text == "conver":
                        conver = True
                    elif grp.text == "x87fpu":
                        x87fpu = True

            for syntax in entry.find_all("syntax"):
                if syntax.mnem is None:
                    continue

                mnem = syntax.mnem.text.lower()
                if mnem not in [oa[0] for oa in OPCODE_ALIASES]:
                    OPCODES.add(mnem)
                    OPCODE_ALIASES.append((mnem, LongOpCode(mnem=mnem)))

                try:
                    op1, op2, op3 = parse_operands(mnem, syntax)
                except UnsupportedOperandError:
                    continue

                # Not implemented
                # The reg field of the ModR/M byte selects a control register (only MOV (0F20, 0F22)).
                if (
                    op1.am == AddressingMode.C
                    or op2.am == AddressingMode.C
                    or op3.am == AddressingMode.C
                ):
                    continue

                # Not implemented
                # The reg field of the ModR/M byte selects a debug register (only MOV (0F21, 0F23)).
                if (
                    op1.am == AddressingMode.D
                    or op2.am == AddressingMode.D
                    or op3.am == AddressingMode.D
                ):
                    continue

                # 16-bit segment registers not used in long mode
                if (
                    op1.am == AddressingMode.S
                    or op2.am == AddressingMode.S
                    or op3.am == AddressingMode.S
                ):
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
                    sec_opcd=sec_opcd,
                    opcd_ext=opcd_ext if opcd_ext is not None else -1,
                    needs_mod_rm=1 if r else 0,
                    note=note,
                    op_size=op_size,
                    direction=direction,
                    acc=int(acc),
                    branch=int(branch),
                    conver=int(conver),
                    x87fpu=int(x87fpu),
                    op1=op1,
                    op2=op2,
                    op3=op3,
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

    sorted_opcode_aliases = sorted(OPCODE_ALIASES, key=lambda oa: oa[0])

    with open(output_path, "w") as f:
        f.write(
            template.render(
                generator=sys.argv[0],
                opcodes=was_opcodes,
                opcode_aliases=sorted_opcode_aliases,
            )
        )


if __name__ == "__main__":
    input_path = sys.argv[1]
    output_path = sys.argv[2]

    x86reference = read_xml(input_path)
    was_opcodes = make_was_opcodes(x86reference)
    output_code(was_opcodes, output_path)
