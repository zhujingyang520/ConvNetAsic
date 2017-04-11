// -----------------------------------------------------------------------------
// This file exports the global function definitions, which is useful for
// verilog module calling
// -----------------------------------------------------------------------------


// ---------------------------
// Ceiling of log2
// ---------------------------
function integer clog2(input integer n);
  integer i;
  begin
    clog2 = 0;
    for (i = n - 1; i > 0; i = i >> 1) begin
      clog2 = clog2 + 1;
    end
  end
endfunction

// ---------------------------------------
// Ceiling of (a/b)
// Note: a & b should be a postive number
// ---------------------------------------
function integer ceil_div(input integer a, input integer b);
  integer i;
  begin
    ceil_div = 1;
    for (i = b; i < a; i = i + b) begin
      ceil_div = ceil_div + 1;
    end
  end
endfunction
