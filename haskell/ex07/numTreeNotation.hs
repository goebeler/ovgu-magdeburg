-- do num <- numberNode x
--    nt1 <- numberTree t1
--    nt2 <- numberTree t2
--    return (Node num nt1 nt2)
--
--
-- numberNode x >>= (\num ->
--      numberTree t1 >>= (\nt1 ->
--          numberTree t2 >>= (\nt2 ->
--              return (Node num nt1 nt2))))
