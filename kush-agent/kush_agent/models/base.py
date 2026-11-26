"""Provider-neutral model interface.

Every adapter speaks the same action vocabulary (the computer toolset members in
`kush_agent.tools.computer.ACTIONS`) and keeps its own provider-specific conversation.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any

from ..tools.computer import ToolResult

NOT_EXECUTED = "Not executed: an earlier computer action in this turn failed."


@dataclass
class Action:
    id: str
    name: str
    input: dict[str, Any]


@dataclass
class Step:
    actions: list[Action] = field(default_factory=list)
    text: str = ""            # what the model said this turn (progress notes / final answer)
    done: bool = False        # model finished (no more actions)
    error: str | None = None  # refusal, truncation, provider error
    usage: dict[str, Any] = field(default_factory=dict)


class ModelAdapter(ABC):
    """One conversation with one model about one task."""

    name: str = "model"

    @abstractmethod
    def start(self, task: str, system: str) -> None: ...

    @abstractmethod
    def step(self) -> Step: ...

    @abstractmethod
    def add_results(self, results: list[tuple[Action, ToolResult]]) -> None: ...

    def add_user_message(self, text: str) -> None:
        """Extra instruction from the user mid-task (e.g. an answer to the agent's question)."""
        raise NotImplementedError


class ModelError(RuntimeError):
    pass
