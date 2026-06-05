-- LLFPL1 Structural Test Core
Identity(Base_Offset, 100)

Identity(Gravity, 9.81)

-- Nested Evaluation Verification
Map(Calculate, x, plus(x, Base_Offset))

Commit(Calculate(50.5))