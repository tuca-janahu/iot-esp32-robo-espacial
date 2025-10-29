from datetime import datetime, timezone
from sqlalchemy import (
    String, Integer, Float, DateTime, UniqueConstraint, CheckConstraint, Index, text
)
from sqlalchemy.orm import Mapped, mapped_column
from .db import Base   # <— com ponto (import relativo)

class Leitura(Base):
    __tablename__ = "leituras"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String, nullable=False)
    idempotency_key: Mapped[str] = mapped_column(String, nullable=False)
    timestamp: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    temperatura_c: Mapped[float | None] = mapped_column(Float)
    umidade_pct:   Mapped[int   | None] = mapped_column(Integer)
    luminosidade:  Mapped[int   | None] = mapped_column(Integer)
    presenca:      Mapped[int   | None] = mapped_column(Integer)
    probabilidade_vida: Mapped[float | None] = mapped_column(Float)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), nullable=False,
        default=lambda: datetime.now(timezone.utc)
    )

    __table_args__ = (
        UniqueConstraint("idempotency_key", name="uq_leituras_idemp"),
        CheckConstraint("presenca IN (0,1) OR presenca IS NULL", name="ck_presenca_bin"),
        Index("idx_leituras_timestamp_desc", text("timestamp DESC")),
        Index("idx_leituras_device_ts", "device_id", text("timestamp DESC")),
    )
