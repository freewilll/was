#!/usr/bin/env python3

# Script to read x86reference.xml and convert it into C data structures

import operator
import sys
from dataclasses import dataclass
from enum import Enum
from pprint import pprint
from typing import List, Set

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
    size: Size


def make_sized_aliases(mnem: str, sizes=None, short_mnem=None):
    if sizes is None:
        sizes = {Size.SIZE08, Size.SIZE16, Size.SIZE32, Size.SIZE64}

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
    **make_sized_aliases("add"),
    **make_sized_aliases("and"),
    **make_sized_aliases("cmp"),
    **make_sized_aliases("div", sizes={Size.SIZE32, Size.SIZE64}),
    **make_sized_aliases("idiv", sizes={Size.SIZE32, Size.SIZE64}),
    **make_sized_aliases("imul"),
    **make_sized_aliases("lea", sizes={Size.SIZE64}),
    **make_sized_aliases("mov"),
    **make_sized_aliases("not"),
    **make_sized_aliases("or"),
    **make_sized_aliases("pop", sizes={Size.SIZE64}),
    **make_sized_aliases("push", sizes={Size.SIZE64}),
    **make_sized_aliases("ret", short_mnem="retn", sizes={Size.SIZE64}),
    **make_sized_aliases("sar"),
    **make_sized_aliases("shl"),
    **make_sized_aliases("shr"),
    **make_sized_aliases("sub"),
    **make_sized_aliases("test"),
    **make_sized_aliases("xor"),
    "call": LongOpCode(mnem="call", size=None),
    "callq": LongOpCode(mnem="call", size=None),
    "jmp": LongOpCode(mnem="jmp", size=None),
    "leave": LongOpCode(mnem="leave", size=None),
    "leaveq": LongOpCode(mnem="leave", size=None),
    "movabsq": LongOpCode(mnem="mov", size=Size.SIZE64),
    "movsbw": LongOpCode(mnem="movsx", size=None),
    "movsbl": LongOpCode(mnem="movsx", size=None),
    "movsbq": LongOpCode(mnem="movsx", size=None),
    "movslq": LongOpCode(mnem="movsxd", size=None),
    "movswl": LongOpCode(mnem="movsx", size=None),
    "movswq": LongOpCode(mnem="movsx", size=None),
    "movzbl": LongOpCode(mnem="movzx", size=None),
    "movzbq": LongOpCode(mnem="movzx", size=None),
    "movzwl": LongOpCode(mnem="movzx", size=None),
    "movzwq": LongOpCode(mnem="movzx", size=None),
    "jb": LongOpCode(mnem="jb", size=None),
    "jnae": LongOpCode(mnem="jnae", size=None),
    "jc": LongOpCode(mnem="jc", size=None),
    "jnb": LongOpCode(mnem="jnb", size=None),
    "jae": LongOpCode(mnem="jae", size=None),
    "jnc": LongOpCode(mnem="jnc", size=None),
    "jz": LongOpCode(mnem="jz", size=None),
    "je": LongOpCode(mnem="je", size=None),
    "jnz": LongOpCode(mnem="jnz", size=None),
    "jne": LongOpCode(mnem="jne", size=None),
    "jbe": LongOpCode(mnem="jbe", size=None),
    "jna": LongOpCode(mnem="jna", size=None),
    "jnbe": LongOpCode(mnem="jnbe", size=None),
    "ja": LongOpCode(mnem="ja", size=None),
    "jo": LongOpCode(mnem="jo", size=None),
    "jno": LongOpCode(mnem="jno", size=None),
    "js": LongOpCode(mnem="js", size=None),
    "jns": LongOpCode(mnem="jns", size=None),
    "jp": LongOpCode(mnem="jp", size=None),
    "jnp": LongOpCode(mnem="jnp", size=None),
    "jl": LongOpCode(mnem="jl", size=None),
    "jnge": LongOpCode(mnem="jnge", size=None),
    "jnl": LongOpCode(mnem="jnl", size=None),
    "jge": LongOpCode(mnem="jge", size=None),
    "jle": LongOpCode(mnem="jle", size=None),
    "jng": LongOpCode(mnem="jng", size=None),
    "jnle": LongOpCode(mnem="jnle", size=None),
    "jg": LongOpCode(mnem="jg", size=None),
}


class AddressingMode(Enum):
    C = "C"  # The reg field of the ModR/M byte selects a control register
    D = "D"  # The reg field of the ModR/M byte selects a debug register
    E = "E"  # Uses ModR/M byte
    G = "G"  # The reg field of the ModR/M byte selects a general register
    I = "I"  # Immediate
    J = "J"  # RIP relative
    H = "H"  # The r/m field of the ModR/M byte always selects a general register, regardless of the mod field
    M = "M"  # The ModR/M byte may refer only to memory
    O = "O"  # Offset
    R = "R"  # Not used
    S = "S"  # Not used
    T = "T"  # The reg field of the ModR/M byte selects a test register (only MOV (0F24, 0F26)).
    Z = "Z"  # The three least-significant bits of the opcode byte selects a general-purpose register


class OperandType(Enum):
    b = "b"  # Byte
    bs = "bs"  # Byte, sign-extended to the size of the destination operand.
    bss = "bss"  # Byte, sign-extended to the size of the stack pointer (for example, PUSH (6A)).
    d = "d"  #  Doubleword
    dqp = "dqp"  # Doubleword, or quadword, promoted by REX.W in 64-bit mode
    q = "q"  # Quad
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

# Operand types that have the operand-size attribute set
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
    prefix: int
    value: int
    opcd_ext: int
    needs_mod_rm: int
    note: str
    op_size: int
    direction: int
    acc: int
    branch: int
    conver: int
    dst: WasOperand
    src: WasOperand

    def __str__(self):
        opcd_ext = str(self.opcd_ext) if self.opcd_ext != -1 else " "
        if self.needs_mod_rm:
            opcd_ext = "r"

        direction = "dD"[self.direction] if self.direction != -1 else " "
        op_size = "wW"[self.op_size] if self.op_size != -1 else " "

        dst_amt = f"{self.dst.am.value if self.dst.am else ''}{self.dst.type.value if self.dst.type else ''}"
        src_amt = f"{self.src.am.value if self.src.am else ''}{self.src.type.value if self.src.type else ''}"

        acc = "a" if self.acc else " "
        return f"{self.prefix if self.prefix != '00' else '  '} {self.value} {direction}{op_size} {'b' if self.branch else ' '}{'c' if self.conver else ' '} {opcd_ext:2s} {acc}  {self.mnem:10s} {dst_amt:5s} {src_amt:5s} {self.note}"


OPCODES = {opcode.mnem for opcode in OPCODE_ALIASES.values()}


def read_xml(input_path: str):
    with open(input_path, "r") as f:
        xml = f.read()

    data = bs4.BeautifulSoup(xml, features="xml")
    x86reference = data.find("x86reference")

    return x86reference


def parse_operands(entry, operand: str) -> WasOperand:
    am = None
    type = None

    for operand in entry.find_all(operand):
        if operand.get("displayed") == "no":
            continue

        am = AddressingMode(operand.a.text) if operand.a else None
        type = OperandType(operand.t.text) if operand.t else None

    return WasOperand.from_am_and_type(am, type)


def parse_pri_opcd(one_byte, prefix):
    was_opcodes = []

    for pri_opcd in one_byte.find_all("pri_opcd"):
        pri_opcd_was_opcodes = []
        found_invalid_flag = False

        value = pri_opcd["value"].lower()

        # To print out the XML for an opcode
        # if value == "...":
        #     print(pri_opcd.prettify())
        #     exit(1)
        # # else:
        # #     continue

        for entry in pri_opcd.find_all("entry"):
            note = entry.note.brief.text if entry.note else None

            attr = entry.get("attr")
            acc = attr == "acc"

            op_size = int(entry.get("op_size", -1))
            direction = int(entry.get("direction", -1))

            # print(note)
            if note is not None and "Invalid Instruction in 64-Bit Mode" in note:
                found_invalid_flag = True
                continue

            opcd_ext = entry.opcd_ext.text if entry.opcd_ext else None

            r = entry.get("r") == "yes"

            branch = False
            for grp2 in entry.find_all("grp2"):
                if grp2.text == "branch":
                    branch = True

            conver = False
            for grp2 in entry.find_all("grp2"):
                if grp2.text == "conver":
                    conver = True

            for syntax in entry.find_all("syntax"):
                if syntax.mnem is not None:
                    mnem = syntax.mnem.text.lower()
                    if mnem not in OPCODES:
                        continue
                else:
                    continue

                src = parse_operands(syntax, "src")
                dst = parse_operands(syntax, "dst")

                # Not implemented
                if dst.am == AddressingMode.C or src.am == AddressingMode.C:
                    continue

                # Not implemented
                if dst.am == AddressingMode.D or src.am == AddressingMode.D:
                    continue

                # Not implemented
                if dst.am == AddressingMode.H or src.am == AddressingMode.H:
                    continue

                # GNU as ignores opcodes with a memory offset addressing mode (a0-a3) , so I will too.
                if dst.am == AddressingMode.O or src.am == AddressingMode.O:
                    continue

                # 16-bit segment registers not used in long mode
                if dst.am == AddressingMode.S or src.am == AddressingMode.S:
                    continue

                # Not implemented
                if dst.am == AddressingMode.T or src.am == AddressingMode.T:
                    continue

                # Not implemented
                if src.am in (OperandType.bs, OperandType.bss):
                    continue

                was_opcode = WasOpcode(
                    mnem=mnem,
                    prefix=prefix,
                    value=value,
                    opcd_ext=opcd_ext if opcd_ext is not None else -1,
                    needs_mod_rm=1 if r else 0,
                    note=note,
                    op_size=op_size,
                    direction=direction,
                    acc=int(acc),
                    branch=int(branch),
                    conver=int(conver),
                    dst=dst,
                    src=src,
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
