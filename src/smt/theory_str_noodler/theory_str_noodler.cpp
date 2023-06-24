/*
The skeleton of this code was obtained by Yu-Fang Chen from https://github.com/guluchen/z3.
Eternal glory to Yu-Fang.
*/

#include <algorithm>
#include <sstream>
#include <iostream>
#include <cmath>
#include "ast/ast_pp.h"
#include "smt/theory_str_noodler/theory_str_noodler.h"
#include "smt/smt_context.h"
#include "smt/smt_model_generator.h"
#include "smt/theory_lra.h"
#include "smt/theory_arith.h"
#include "smt/smt_context.h"
#include "ast/seq_decl_plugin.h"
#include "ast/reg_decl_plugins.h"
#include "decision_procedure.h"
#include <mata/nfa.hh>


namespace smt::noodler {

    namespace {
        bool IN_CHECK_FINAL = false;
    }

    theory_str_noodler::theory_str_noodler(context& ctx, ast_manager & m, theory_str_noodler_params const & params):
        theory(ctx, ctx.get_manager().mk_family_id("seq")),
        m_params(params),
        m_rewrite(m),
        m_util_a(m),
        m_util_s(m),
        state_len(),
        m_length(m),
        axiomatized_instances() {
    }

    void theory_str_noodler::display(std::ostream &os) const {
        os << "theory_str display" << std::endl;
    }

    void theory_str_noodler::init() {
        theory::init();
        STRACE("str", if (!IN_CHECK_FINAL) tout << "init\n";);
    }

    enode *theory_str_noodler::ensure_enode(expr *e) {
        if (!ctx.e_internalized(e)) {
            ctx.internalize(e, false);
        }
        enode *n = ctx.get_enode(e);
        ctx.mark_as_relevant(n);
        return n;
    }

    theory_var theory_str_noodler::mk_var(enode *const n) {
        if (!m_util_s.is_seq(n->get_expr()) &&
            !m_util_s.is_re(n->get_expr())) {
            return null_theory_var;
        }
        if (is_attached_to_var(n)) {
            return n->get_th_var(get_id());
        } else {
            theory_var v = theory::mk_var(n);
            get_context().attach_th_var(n, this, v);
            get_context().mark_as_relevant(n);
            return v;
        }
    }


    bool theory_str_noodler::internalize_atom(app *const atom, const bool gate_ctx) {
        (void) gate_ctx;
        STRACE("str", tout << "internalize_atom: gate_ctx is " << gate_ctx << ", "
                           << mk_pp(atom, get_manager()) << '\n';);
        context &ctx = get_context();
        if (ctx.b_internalized(atom)) {
            STRACE("str", tout << "done before\n";);
            return true;
        }
        return internalize_term(atom);
    }

    bool theory_str_noodler::internalize_term(app *const term) {
        context &ctx = get_context();

        if (m_util_s.str.is_in_re(term)) {
            if (!ctx.e_internalized(term->get_arg(0))) {
                ctx.internalize(term->get_arg(0), false);
                enode* enode = ctx.get_enode(term->get_arg(0));
                mk_var(enode);
            }
        }

        if (ctx.e_internalized(term)) {
            enode *e = ctx.get_enode(term);
            mk_var(e);
            return true;
        }
        for (auto arg : *term) {
            mk_var(ensure_enode(arg));
        }
        if (m.is_bool(term)) {
            bool_var bv = ctx.mk_bool_var(term);
            ctx.set_var_theory(bv, get_id());
            //We do not want to mark as relevant because it involves
            // irrelevant RE solutions comming from the underlying SAT solver.
            //ctx.mark_as_relevant(bv);
        }

        enode *e = nullptr;
        if (ctx.e_internalized(term)) {
            e = ctx.get_enode(term);
        } else {
            e = ctx.mk_enode(term, false, m.is_bool(term), true);
        }
        mk_var(e);
        if (!ctx.relevancy()) {
            relevant_eh(term);
        }
        return true;
    }

    void theory_str_noodler::apply_sort_cnstr(enode *n, sort *s) {
        mk_var(n);
    }

    void theory_str_noodler::print_ctx(context &ctx) {  // test_hlin
        ast_manager m = get_manager();
        expr_ref_vector dis(m);
        expr_ref_vector vars(m);
        expr_ref_vector cons(m);
        expr_ref_vector unfixed(m);

        expr_ref_vector con(get_manager());
        expr_ref_vector un(get_manager());

        ctx.get_assignments(con);
        std::cout << "Assignment :" << con.size() << " " << un.size() << std::endl;
        for(expr* e : con) {
            std::cout << "OOO " << mk_pp(e, m) << " " << ctx.is_relevant(e) << " " << ctx.b_internalized(e) << " " << ctx.find_assignment(e);
            if(ctx.b_internalized(e)) {
                std::cout << ctx.get_bool_var(e);
            }
            std::cout << std::endl;
        }

        return;

        std::cout << ctx.get_unsat_core_size() << std::endl;
        std::cout << "~~~~~ print ctx start ~~~~~~~\n";
//        context& ctx = get_context();
        unsigned nFormulas = ctx.get_num_asserted_formulas();
        expr_ref_vector Literals(get_manager()), Assigns(get_manager());
        ctx.get_guessed_literals(Literals);
        ctx.get_assignments(Assigns);
        std::cout << "~~~~~~ from get_asserted_formulas ~~~~~~\n";
        for (unsigned i = 0; i < nFormulas; ++i) {
            expr *ex = ctx.get_asserted_formula(i);
            std::cout << mk_pp(ex, get_manager()) << " relevant? " << ctx.is_relevant(ex) << std::endl;
        }
        std::cout << "~~~~~~ from get_guessed_literals ~~~~~~\n";
        for (auto &e : Literals) {
            std::cout << mk_pp(e, get_manager()) << " relevant? " << ctx.is_relevant(e) << std::endl;
        }
        std::cout << "~~~~~~ from get_assignments ~~~~~~\n";
        for (auto &e : Assigns) {
//            print_ast(e);
            std::cout << mk_pp(e, get_manager()) << " relevant? " << ctx.is_relevant(e) << std::endl;
        }
        std::cout << "~~~~~ print ctx end ~~~~~~~~~\n";
    }

    void theory_str_noodler::print_ast(expr *e) {  // test_hlin
        ast_manager m = get_manager();
        unsigned int id = e->get_id();
        ast_kind ast = e->get_kind();
        sort *e_sort = e->get_sort();
        sort *bool_sort = m.mk_bool_sort();
        sort *str_sort = m_util_s.str.mk_string_sort();
        std::cout << "#e.id = " << id << std::endl;
        std::cout << "#e.kind = " << get_ast_kind_name(ast) << std::endl;
        std::cout << std::boolalpha << " sort? " << (e_sort == bool_sort) << std::endl;
        std::cout << std::boolalpha << "#is string sort? " << (e_sort == str_sort) << std::endl;
        std::cout << std::boolalpha << "#is string term? " << m_util_s.str.is_string(e) << std::endl;
        std::cout << std::boolalpha << "#is_numeral? " << m_util_a.is_numeral(e) << std::endl;
        std::cout << std::boolalpha << "#is_to_int? " << m_util_a.is_to_int(e) << std::endl;
        std::cout << std::boolalpha << "#is_int_expr? " << m_util_a.is_int_expr(e) << std::endl;
        std::cout << std::boolalpha << "#is_le? " << m_util_a.is_le(e) << std::endl;
        std::cout << std::boolalpha << "#is_ge? " << m_util_a.is_ge(e) << std::endl;
    }


    void theory_str_noodler::init_search_eh() {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);
        context &ctx = get_context();
        unsigned nFormulas = ctx.get_num_asserted_formulas();
        for (unsigned i = 0; i < nFormulas; ++i) {
            obj_hashtable<app> lens;
            util::get_len_exprs(to_app(ctx.get_asserted_formula(i)), m_util_s, m, lens);
            for (app* const a : lens) {
                util::get_str_variables(a, this->m_util_s, m, this->len_vars, &this->predicate_replace);
            }

            expr *ex = ctx.get_asserted_formula(i);
            ctx.mark_as_relevant(ex);
            string_theory_propagation(ex, true, false);
            
        }
        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);

    }

    void theory_str_noodler::string_theory_propagation(expr *expr, bool init, bool neg) {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);
        STRACE("str", tout << mk_pp(expr, get_manager()) << std::endl;);

        context &ctx = get_context();

        if (!ctx.e_internalized(expr)) {
            ctx.internalize(expr, false);
        }
        //We do not mark the expression as relevant since we do not want bias a
        //fresh SAT solution by the newly added theory axioms.
        // enode *n = ctx.get_enode(expr);
        // ctx.mark_as_relevant(n);

        if(m.is_not(expr)) {
            neg = !neg;
        }

        // TODO weird, we have to do it because inequations are handled differently as equations, and they might not have been set as relevant
        if(init && m.is_eq(expr) && neg) {
            ctx.mark_as_relevant(m.mk_not(expr));
        }

        sort *expr_sort = expr->get_sort();
        sort *str_sort = m_util_s.str.mk_string_sort();

        if (expr_sort == str_sort) {

            enode *n = ctx.get_enode(expr);
            propagate_basic_string_axioms(n);
            if (is_app(expr) && m_util_s.str.is_concat(to_app(expr))) {
                propagate_concat_axiom(n);
            }
        }
        // if expr is an application, recursively inspect all arguments
        if (is_app(expr) && !m_util_s.str.is_length(expr)) {
            app *term = to_app(expr);
            unsigned num_args = term->get_num_args();
            for (unsigned i = 0; i < num_args; i++) {
                string_theory_propagation(term->get_arg(i), init, neg);
            }
        }

        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);

    }

    // for concatenation xy create axiom |xy| = |x| + |y| where x, y are some string expressions
    void theory_str_noodler::propagate_concat_axiom(enode *cat) {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);


        app *a_cat = cat->get_expr();
        SASSERT(m_util_s.str.is_concat(a_cat));
        ast_manager &m = get_manager();

        // build LHS
        expr_ref len_xy(m);
        len_xy = m_util_s.str.mk_length(a_cat);
        SASSERT(len_xy);

        // build RHS: start by extracting x and y from Concat(x, y)
        SASSERT(a_cat->get_num_args() == 2);
        app *a_x = to_app(a_cat->get_arg(0));
        app *a_y = to_app(a_cat->get_arg(1));
        expr_ref len_x(m);
        len_x = m_util_s.str.mk_length(a_x);
        SASSERT(len_x);

        expr_ref len_y(m);
        len_y = m_util_s.str.mk_length(a_y);
        SASSERT(len_y);

        // now build len_x + len_y
        expr_ref len_x_plus_len_y(m);
        len_x_plus_len_y = m_util_a.mk_add(len_x, len_y);
        SASSERT(len_x_plus_len_y);

        STRACE("str-concat",
            tout << "[Concat Axiom] " << mk_pp(len_xy, m) << " = " << mk_pp(len_x, m) << " + " << mk_pp(len_y, m)
                 << std::endl;
        );

        // finally assert equality between the two subexpressions
        app_ref eq(m.mk_eq(len_xy, len_x_plus_len_y), m);
        //ctx.internalize(eq, false);
        SASSERT(eq);
        add_axiom(eq);
        // std::cout << mk_pp(eq, m) << std::endl;
        this->axiomatized_len_axioms.push_back(eq);
        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);

    }

    void theory_str_noodler::propagate_basic_string_axioms(enode *str) {
        context &ctx = get_context();
        ast_manager &m = get_manager();

        {
            sort *a_sort = str->get_expr()->get_sort();
            sort *str_sort = m_util_s.str.mk_string_sort();
            if (a_sort != str_sort) {
                STRACE("str",
                       tout << "WARNING: not setting up string axioms on non-string term " << mk_pp(str->get_expr(), m)
                            << std::endl;);
                return;
            }
        }

        // TESTING: attempt to avoid a crash here when a variable goes out of scope
        if (str->get_iscope_lvl() > ctx.get_scope_level()) {
            STRACE("str", tout << "WARNING: skipping axiom setup on out-of-scope string term" << std::endl;);
            return;
        }

        // generate a stronger axiom for constant strings
        app_ref a_str(str->get_expr(), m);

        if (m_util_s.str.is_string(a_str)) {
            STRACE("str-axiom", tout << "[ConstStr Axiom] " << mk_pp(a_str, m) << std::endl);

            expr_ref len_str(m_util_s.str.mk_length(a_str), m);
            SASSERT(len_str);

            zstring strconst;
            m_util_s.str.is_string(str->get_expr(), strconst);
            unsigned int l = strconst.length();
            expr_ref len(m_util_a.mk_numeral(rational(l), true), m);

            expr_ref eq(m.mk_eq(len_str, len), m);
            add_axiom(eq);
            return;
        } else if(!m.is_ite(a_str)) {
            // build axiom 1: Length(a_str) >= 0
            { 
                /**
                 * FIXME: fix some day. Based on some expriments it is better to introduce this axiom when returning 
                 * length formula from the decision procedure. If the axiom was introduced here, it leads to solving 
                 * more equations (not exactly sure why, maybe due to the cooperation with LIA solver, maybe it is not 
                 * properly relevancy propagated...)
                 */
                //return; 
                STRACE("str-axiom", tout << "[Non-Zero Axiom] " << mk_pp(a_str, m) << std::endl);

                // build LHS
                expr_ref len_str(m);
                len_str = m_util_s.str.mk_length(a_str);
                SASSERT(len_str);
                // build RHS
                expr_ref zero(m);
                zero = m_util_a.mk_numeral(rational(0), true);
                SASSERT(zero);
                // build LHS >= RHS and assert
                app_ref lhs_ge_rhs(m_util_a.mk_ge(len_str, zero), m);
                ctx.internalize(lhs_ge_rhs, false);
                SASSERT(lhs_ge_rhs);
                STRACE("str", tout << "string axiom 1: " << mk_ismt2_pp(lhs_ge_rhs, m) << std::endl;);

                add_axiom({mk_literal(lhs_ge_rhs)});
                
 
                this->axiomatized_len_axioms.push_back(lhs_ge_rhs);
                    

                
                
                //return;
            }

            // build axiom 2: Length(a_str) == 0 <=> a_str == ""
            {
                // return;
                STRACE("str-axiom", tout << "[Zero iff Empty Axiom] " << mk_pp(a_str, m) << std::endl);

                // build LHS of iff
                expr_ref len_str(m);
                len_str = m_util_s.str.mk_length(a_str);
                SASSERT(len_str);
                expr_ref zero(m);
                zero = m_util_a.mk_numeral(rational(0), true);
                SASSERT(zero);
                expr_ref lhs(m);
                lhs = m_util_a.mk_le(len_str, zero);
                SASSERT(lhs);
                // build RHS of iff
                expr_ref empty_str(m);
                empty_str = m_util_s.str.mk_empty(a_str->get_sort());
                SASSERT(empty_str);
                expr_ref rhs(m);
                rhs = m.mk_eq(a_str, empty_str);
                ctx.internalize(rhs, false);
                ctx.internalize(lhs, false);
                // ctx.mark_as_relevant(rhs.get());
                // ctx.mark_as_relevant(lhs.get());
                SASSERT(rhs);
                // build LHS <=> RHS and assert
                add_axiom(m.mk_or(m.mk_not(lhs), rhs));
            }

        } else {
            // INFO do nothing if ite, because this function is called only in string_theory_propagation where ite is processed
        }
    }

    void theory_str_noodler::add_length_axiom(expr *n) {
        app_ref ln(m_util_a.mk_ge(n, m_util_a.mk_int(0)), m);
        ctx.internalize(ln, false);
        add_axiom(ln);
        this->axiomatized_len_axioms.push_back(ln);
    }

    void theory_str_noodler::relevant_eh(app *const n) {
        STRACE("str", tout << "relevant: " << mk_pp(n, get_manager()) << " with family id " << n->get_family_id() << ", sort " << n->get_sort()->get_name() << " and decl kind " << n->get_decl_kind() << std::endl;);

        if (m_util_s.str.is_length(n)) { // str.len
            add_length_axiom(n);

            // FIXME what is this? is it important? can we delete this?
            expr *arg;
            if (m_util_s.str.is_length(n, arg) && !has_length(arg) && get_context().e_internalized(arg)) {
                enforce_length(arg);
            }
        } else if(m_util_s.str.is_lt(n)) { // str.<
            util::throw_error("str.< is not supported");
        } else if(m_util_s.str.is_le(n)) { // str.<=
            util::throw_error("str.<= is not supported");
        } else if (m_util_s.str.is_at(n)) { // str.at
            handle_char_at(n);
        } else if (m_util_s.str.is_extract(n)) { // str.substr
            handle_substr(n);
        } else if(m_util_s.str.is_prefix(n)) { // str.prefixof
            handle_prefix(n);
            handle_not_prefix(n);
        } else if(m_util_s.str.is_suffix(n)) { // str.suffixof
            handle_suffix(n);
            handle_not_suffix(n);
        } else if(m_util_s.str.is_contains(n)) { // str.contains
            handle_contains(n);
            handle_not_contains(n);
        } else if (m_util_s.str.is_index(n)) { // str.indexof
            handle_index_of(n);
        } else if (m_util_s.str.is_replace(n)) { // str.replace
            handle_replace(n);
        } else if(m_util_s.str.is_replace_all(n)) { // str.replace_all
            util::throw_error("str.replace_all is not supported");
        } else if(m_util_s.str.is_replace_re(n)) { // str.replace_re
            util::throw_error("str.replace_re is not supported");
        } else if(m_util_s.str.is_replace_re_all(n)) { // str.replace_re_all
            util::throw_error("str.replace_re_all is not supported");
        } else if (m_util_s.str.is_is_digit(n)) { // str.is_digit
            util::throw_error("str.is_digit is not supported");
        } else if (m_util_s.str.is_to_code(n)) { // str.to_code
            util::throw_error("str.to_code is not supported");
        } else if (m_util_s.str.is_from_code(n)) { // str.from_code
            util::throw_error("str.from_code is not supported");
        } else if (m_util_s.str.is_stoi(n)) { // str.to_int
            util::throw_error("str.to_int is not supported");
        } else if (m_util_s.str.is_itos(n)) { // str.from_int
            util::throw_error("str.from_int is not supported");
        } else if (
            m_util_s.str.is_concat(n) || // str.++
            m_util_s.re.is_to_re(n) || // str.to_re
            m_util_s.str.is_in_re(n) || // str.in_re
            m_util_s.is_re(n) || // one of re. command (re.none, re.all, re.comp, ...)
            is_str_variable(n) || // string variable
            // RegLan variables should never occur here, they are always eliminated by rewriter I think
            m_util_s.str.is_string(n) // string literal
        ) {
            // we do not need to handle these, concatenation is handled differently (TODO: explain better?)
            // and everything else will be handled during final_check_eh
        } else {
            util::throw_error("Unknown predicate/function from string theory (this should not happen)");
        }

    }

    /*
    ensure that all elements in equivalence class occur under an application of 'length'
    */
    void theory_str_noodler::enforce_length(expr *e) {
        enode *n = ensure_enode(e);
        enode *n1 = n;
        do {
            expr *o = n->get_expr();
            // TODO is this needed? what happens if we get ite, it does not do anything
            if (!has_length(o) && !m.is_ite(o)) {
                expr_ref len = mk_len(o);
                add_length_axiom(len);
            }
            n = n->get_next();
        } while (n1 != n);
    }

    void theory_str_noodler::assign_eh(bool_var v, const bool is_true) {
        ast_manager &m = get_manager();
        STRACE("str", tout << "assign: bool_var #" << v << " is " << is_true << ", "
                            << mk_pp(get_context().bool_var2expr(v), m) << "@ scope level:" << m_scope_level << '\n';);
        context &ctx = get_context();
        expr *e = ctx.bool_var2expr(v);
        expr *e1 = nullptr, *e2 = nullptr;
        if (m_util_s.str.is_prefix(e, e1, e2)) {
            // INFO done in relevant_eh, because there was some problem with adding axioms, Z3 added wrong axiom if it was done here and not in relevant_eh for some reason
            // INFO in seq_theory they do similar stuff, and they do it here and not in relevant_eh, they also use skolem functions to handle it, might be worth to investigate

            // if (is_true) {
            //     handle_prefix(e);
            // } else {
            //     util::throw_error("unsupported predicate");
            //     //handle_not_prefix(e);
            // }
        } else if (m_util_s.str.is_suffix(e, e1, e2)) {
            // INFO done in relevant_eh, because there was some problem with adding axioms, Z3 added wrong axiom if it was done here and not in relevant_eh for some reason
            // INFO in seq_theory they do similar stuff, and they do it here and not in relevant_eh, they also use skolem functions to handle it, might be worth to investigate

            // if (is_true) {
            //     handle_suffix(e);
            // } else {
            //     util::throw_error("unsupported predicate");
            //     // handle_not_suffix(e);
            // }
        } else if (m_util_s.str.is_contains(e, e1, e2)) {
            // FIXME could the problem from previous two also occur here? if yes, handle_contains and handle_not_contains should be in relevant_eh BUT m_not_contains_todo should be updated probably here (if applicable), so we can return unknown
            if (is_true) {
                handle_contains(e);
            } else {
                handle_not_contains(e);
            }
        } else if (m_util_s.str.is_in_re(e)) {
            // INFO the problem from previous cannot occur here - Vojta
            handle_in_re(e, is_true);
        } else if(m.is_bool(e)) {
            ensure_enode(e);
            TRACE("str", tout << "bool literal " << mk_pp(e, m) << " " << is_true << "\n" );
            // UNREACHABLE();
        } else {
            TRACE("str", tout << "unhandled literal " << mk_pp(e, m) << "\n";);
            UNREACHABLE();
        }
    }

    void theory_str_noodler::new_eq_eh(theory_var x, theory_var y) {
        // get the expressions for left and right side of equation
        expr_ref l{get_enode(x)->get_expr(), m};
        expr_ref r{get_enode(y)->get_expr(), m};

        app* equation = m.mk_eq(l, r);

        // TODO explain what is happening here
        if(!ctx.e_internalized(equation)) {
            ctx.mark_as_relevant(equation);
        }

        if(m_util_s.is_re(l) && m_util_s.is_re(r)) { // language equation
            m_lang_eq_todo.push_back({l, r});
        } else { // word equation
            // mk_eq_atom can check if equation trivially holds (by having the
            // same thing on both sides) or not (by having two distintict
            // string literals)
            app* equation_atom = ctx.mk_eq_atom(l, r);
            if (m.is_false(equation_atom)) {
                // if we have two distinct literals, we immediately stop by not allowing this equation
                add_axiom({mk_literal(m.mk_not(equation))});
            } else if (!m.is_true(equation_atom)) {
                // if equation is not trivially true, we add it for later check
                m_word_eq_todo.push_back({l, r});

                // Optimization: If equation holds, then the lengths of both sides must be the same.
                // We do this only if the equation (or its inverse) is already for sure relevant,
                // otherwise adding the axiom might make the equation relevant (even though it is not).
                // Used for quick check for arith solver, to immediately realise that sides cannot be
                // ever equal based on lengths.
                // This does NOT add the variables from the equation to len_vars.
                if (ctx.is_relevant(equation) || ctx.is_relevant(m.mk_eq(r, l))) {
                    literal l_eq_r = mk_literal(equation);    //mk_eq(l, r, false);
                    literal len_l_eq_len_r = mk_eq(m_util_s.str.mk_length(l), m_util_s.str.mk_length(r), false);
                    add_axiom({~l_eq_r, len_l_eq_len_r});
                }
            }
        }

        STRACE("str", tout << "new_eq: " << l <<  " = " << r << std::endl;);
    }

    void theory_str_noodler::new_diseq_eh(theory_var x, theory_var y) {
        // get the expressions for left and right side of disequation
        const expr_ref l{get_enode(x)->get_expr(), m};
        const expr_ref r{get_enode(y)->get_expr(), m};

        app* equation = m.mk_eq(l, r);
        app* disequation = m.mk_not(equation);

        // This is to handle the case containing ite inside disequations
        // TODO explain better
        if(!ctx.e_internalized(equation)) {
            STRACE("str", tout << "relevanting: " << mk_pp(disequation, m) << std::endl;);
            ctx.mark_as_relevant(disequation);
        }
        ctx.internalize(disequation, false);

        if(m_util_s.is_re(l) && m_util_s.is_re(r)) { // language disequation
            m_lang_diseq_todo.push_back({l, r});
        } else { // word disequation
            // mk_eq_atom can check if equation trivially holds (by having the
            // same thing on both sides) or not (by having two distintict
            // string literals)
            app* equation_atom = ctx.mk_eq_atom(l, r);
            if (m.is_true(equation_atom)) {
                // if equation trivially holds (i.e. this disequation does not),
                // we immediately stop by always forcing it
                add_axiom({mk_literal(equation)});
            } else if (!m.is_false(equation_atom)) {
                // if equation is not trivially false, we add it for later check
                m_word_diseq_todo.push_back({l, r});
            }
        }

        STRACE("str",
            tout << ctx.find_assignment(equation) << " " << ctx.find_assignment(disequation) << std::endl
                 << "new_diseq: " << l << " != " << r
                 << " @" << m_scope_level<< " " << ctx.get_bool_var(equation) << " "
                 << ctx.is_relevant(disequation) << ":" << ctx.is_relevant(equation) << std::endl;
        );
    }

    bool theory_str_noodler::can_propagate() {
        return false;
    }

    void theory_str_noodler::propagate() {
        // STRACE("str", if (!IN_CHECK_FINAL) tout << "o propagate" << '\n';);

        // for(const expr_ref& ex : this->len_state_axioms)
        //     add_axiom(ex);
    }

    void theory_str_noodler::push_scope_eh() {
        m_scope_level += 1;
        m_word_eq_todo.push_scope();
        m_lang_eq_todo.push_scope();
        m_lang_diseq_todo.push_scope();
        m_word_diseq_todo.push_scope();
        m_membership_todo.push_scope();
        m_not_contains_todo.push_scope();
        STRACE("str", if (!IN_CHECK_FINAL) tout << "push_scope: " << m_scope_level << '\n';);
    }

    void theory_str_noodler::pop_scope_eh(const unsigned num_scopes) {
        // remove all axiomatized terms
        axiomatized_terms.reset();
        m_scope_level -= num_scopes;
        m_word_eq_todo.pop_scope(num_scopes);
        m_lang_eq_todo.pop_scope(num_scopes);
        m_lang_diseq_todo.pop_scope(num_scopes);
        m_word_diseq_todo.pop_scope(num_scopes);
        m_membership_todo.pop_scope(num_scopes);
        m_not_contains_todo.pop_scope(num_scopes);
        m_rewrite.reset();
        STRACE("str", if (!IN_CHECK_FINAL)
            tout << "pop_scope: " << num_scopes << " (back to level " << m_scope_level << ")\n";);
    }

    void theory_str_noodler::reset_eh() {
        // FIXME should here be something?
        STRACE("str", tout << "reset" << '\n';);
    }



    zstring theory_str_noodler::print_word_term(expr * e) const{
        zstring s;
        if (m_util_s.str.is_string(e, s)) {
            return s;
        }
        if (m_util_s.str.is_concat(e)) {
            //e is a concat of some elements
            for (unsigned i = 0; i < to_app(e)->get_num_args(); i++) {
                s = s+ print_word_term(to_app(e)->get_arg(i));
            }
            return s;
        }
        if (m_util_s.str.is_stoi(e)){
            return zstring("stoi");
        }
        if (m_util_s.str.is_itos(e)){
            return zstring("itos");
        }

        // e is a string variable
        std::stringstream ss;
        ss << "V("<<mk_pp(e, get_manager())<<")";
        s = zstring(ss.str().data());
        return s;

    }

    void theory_str_noodler::remove_irrelevant_constr() {
        STRACE("str", tout << "Remove irrevelant" << std::endl);

        this->m_word_eq_todo_rel.clear();
        this->m_word_diseq_todo_rel.clear();
        this->m_membership_todo_rel.clear();
        this->m_lang_eq_todo_rel.clear();

        for (const auto& we : m_word_eq_todo) {
            app_ref eq(m.mk_eq(we.first, we.second), m);
            app_ref eq_rev(m.mk_eq(we.second, we.first), m);

            STRACE("str",
                tout << "  Eq " << mk_pp(eq.get(), m) << " is " << (ctx.is_relevant(eq.get()) ? "" : "not ") << "relevant"
                     << " with assignment " << ctx.find_assignment(eq.get())
                     << " and its reverse is " << (ctx.is_relevant(eq_rev.get()) ? "" : "not ") << "relevant" << std::endl;
            );
            
            // check if equation or its reverse are relevant (we check reverse to be safe) and...
            if((ctx.is_relevant(eq.get()) || ctx.is_relevant(eq_rev.get())) &&
               // ...neither equation nor its reverse are saved as relevant yet
               !this->m_word_eq_todo_rel.contains(we) && !this->m_word_eq_todo_rel.contains({we.second, we.first})
               ) {
                // save it as relevant
                this->m_word_eq_todo_rel.push_back(we);
            }
        }

        for (const auto& wd : m_word_diseq_todo) {
            app_ref dis(m.mk_not(m.mk_eq(wd.first, wd.second)), m);
            app_ref dis_rev(m.mk_not(m.mk_eq(wd.second, wd.first)), m);

            STRACE("str",
                tout << "  Diseq " << mk_pp(dis.get(), m) << " is " << (ctx.is_relevant(dis.get()) ? "" : "not ") << "relevant"
                     << " with assignment " << ctx.find_assignment(dis.get())
                     << " and its reverse is " << (ctx.is_relevant(dis_rev.get()) ? "" : "not ") << "relevant" << std::endl;
            );
            
            // check if disequation or its reverse are relevant (we check reverse to be safe) and...
            if((ctx.is_relevant(dis.get()) || ctx.is_relevant(dis_rev.get())) &&
               // ...neither disequation nor its reverse are saved as relevant yet
               !this->m_word_diseq_todo_rel.contains(wd) && !this->m_word_diseq_todo_rel.contains({wd.second, wd.first})
               ) {
                // save it as relevant
                this->m_word_diseq_todo_rel.push_back(wd);
            }
        }

        for (const auto& memb : this->m_membership_todo) {
            app_ref memb_app(m_util_s.re.mk_in_re(std::get<0>(memb), std::get<1>(memb)), m);
            app_ref memb_app_orig(m_util_s.re.mk_in_re(std::get<0>(memb), std::get<1>(memb)), m);
            if(!std::get<2>(memb)){
                memb_app = m.mk_not(memb_app);

            }
            
            STRACE("str",
                tout << "  " << mk_pp(memb_app.get(), m) << " is " << (ctx.is_relevant(memb_app.get()) ? "" : "not ") << "relevant"
                     << " with assignment " << ctx.find_assignment(memb_app.get())
                     << ", " << mk_pp(memb_app_orig.get(), m) << " is " << (ctx.is_relevant(memb_app_orig.get()) ? "" : "not ") << "relevant"
                     << std::endl;
            );

            // check if membership (or if we have negation, its negated form) is relevant and...
            if((ctx.is_relevant(memb_app.get()) || ctx.is_relevant(memb_app_orig.get())) &&
               // this membership constraint is not added to relevant yet
               !this->m_membership_todo_rel.contains(memb)
               ) {
                this->m_membership_todo_rel.push_back(memb);
            }
        }

        // TODO check for relevancy of language (dis)equations, right now we assume everything is relevant
        for(const auto& we : m_lang_eq_todo) {
            this->m_lang_eq_todo_rel.push_back({we.first, we.second, true});
        }
        for(const auto& we : m_lang_diseq_todo) {
            this->m_lang_eq_todo_rel.push_back({we.first, we.second, false});
        }
    }

    /*
    Final check for an assignment of the underlying boolean skeleton.
    */
    final_check_status theory_str_noodler::final_check_eh() {
        TRACE("str", tout << "final_check starts" << std::endl;);

        remove_irrelevant_constr();

        STRACE("str",
            tout << "Relevant predicates:" << std::endl;
            tout << "  - eqs(" << this->m_word_eq_todo_rel.size() << "):" << std::endl;
            for (const auto &we: this->m_word_eq_todo_rel) {
                tout << "    " << mk_pp(we.first, m) << " == " << mk_pp(we.second, m) << std::endl;
            }
            tout << "  - diseqs(" << this->m_word_diseq_todo_rel.size() << "):" << std::endl;
            for (const auto &we: this->m_word_diseq_todo_rel) {
                tout << "    " << mk_pp(we.first, m) << " != " << mk_pp(we.second, m) << std::endl;
            }
            tout << "  - membs(" << this->m_membership_todo_rel.size() << "):" << std::endl;
            for (const auto &we: this->m_membership_todo_rel) {
                tout << "    " << mk_pp(std::get<0>(we), m) << (std::get<2>(we) ? "" : " not") << " in " << mk_pp(std::get<1>(we), m) << std::endl;
            }
            tout << "  - lang (dis)eqs(" << this->m_lang_eq_todo_rel.size() << "):" << std::endl;
            for (const auto &we: this->m_lang_eq_todo_rel) {
                tout << "    " << mk_pp(std::get<0>(we), m) << (std::get<2>(we) ? " == " : " != ") << mk_pp(std::get<1>(we), m) << std::endl;
            }
        );

        // difficult not(contains) predicates -> unknown
        // TODO: should we check if any of those not contains are relevant? if we are not planning to check for relevancy, we can just throw error when we are processsing not contains in relevant_eh() and not here
        if(!this->m_not_contains_todo.empty()) {
            STRACE("str", tout << "giving up, there is not contains" << std::endl);
            return FC_GIVEUP;
        }


        /********************************* LANGAGUGE (DIS)EQUATIONS **************************/
        for(const auto& item : this->m_lang_eq_todo_rel) {
            // RegLan variables should not occur here, they are eliminated by z3 rewriter I think,
            // so both sides of the (dis)equations should be terms representing reg. languages
            auto left_side = std::get<0>(item);
            auto right_side = std::get<1>(item);
            bool is_equation = std::get<2>(item);

            STRACE("str",
                tout << "Checking lang (dis)eq: " << mk_pp(left_side, m) << (is_equation ? " == " : " != ") << mk_pp(right_side, m) << std::endl;
            );

            // get symbols from both sides
            std::set<uint32_t> alphabet;
            util::extract_symbols(left_side, m_util_s, m, alphabet);
            util::extract_symbols(right_side, m_util_s, m, alphabet);

            // construct NFAs for both sides
            Mata::Nfa::Nfa nfa1 = util::conv_to_nfa(to_app(left_side), m_util_s, m, alphabet, false );
            Mata::Nfa::Nfa nfa2 = util::conv_to_nfa(to_app(right_side), m_util_s, m, alphabet, false );

            // check if NFAs are equivalent (if we have equation) or not (if we have disequation)
            bool are_equiv = Mata::Nfa::are_equivalent(nfa1, nfa2);
            if ((is_equation && !are_equiv) || (!is_equation && are_equiv)) {
                // the language (dis)equation does not hold => block it and return
                app_ref lang_eq(ctx.mk_eq_atom(left_side, right_side), m);
                if(is_equation){
                    add_axiom(m.mk_not(lang_eq));
                } else {
                    add_axiom(lang_eq);
                }
                return FC_DONE;
            }
        }
        /**************************************************************************************/


        /********************* GATHER WORD (DIS)EQUATIONS ***********************/

        // gathered (dis)eqs as z3 exprs
        obj_hashtable<app> eqs_and_diseqs;
        // gathered (dis)eqs that can be simplified into false (for example "aa" = "bbb")
        std::vector<expr*> false_eqs_and_diseqs;

        for (const auto &we: this->m_word_eq_todo_rel) {
            app *const e = ctx.mk_eq_atom(we.first, we.second);
            // mk_eq_atom can simplify to false/true (for example if we have "aa" = "bbb" it simplifies it into false)
            if(m.is_false(e)) {
                false_eqs_and_diseqs.push_back(m.mk_eq(we.first, we.second));
            } else if (!m.is_true(e)) {
                eqs_and_diseqs.insert(e);
            }
        }

        for (const auto& we : this->m_word_diseq_todo_rel) {
            app *const e = m.mk_not(ctx.mk_eq_atom(we.first, we.second));
            // mk_eq_atom can simplify to false/true (for example if we have "aa" = "bbb" it simplifies it into false)
            if(m.is_false(e)) {
                false_eqs_and_diseqs.push_back(m.mk_not(m.mk_eq(we.first, we.second)));
            } else if (!m.is_true(e)) {
                eqs_and_diseqs.insert(e);
            }
        }
        
        // if we have some (dis)eq that can be simplified into false, we immediately quit
        if(!false_eqs_and_diseqs.empty()) {
            for (const auto &e : false_eqs_and_diseqs) {
                // we add that this (dis)eq cannot hold as axiom, so that sat solver does not return same assignment
                add_axiom(m.mk_not(e));
            }
            return FC_CONTINUE;
        }

        // from z3 (dis)equations to ours
        Formula instance;
        this->conj_instance(eqs_and_diseqs, instance);
        STRACE("str",
            for(const auto& f : instance.get_predicates()) {
                tout << f.to_string() << std::endl;
            }
        );

        /***************** FINISH GATHERING WORD (DIS)EQUATIONS ******************/


        /********************** GATHER SYMBOLS **********************/

        std::set<uint32_t> symbols_in_formula{};
        for (const auto &word_equation: m_word_eq_todo_rel) {
            util::extract_symbols(word_equation.first, m_util_s, m, symbols_in_formula);
            util::extract_symbols(word_equation.second, m_util_s, m, symbols_in_formula);
        }

        for (const auto &word_equation: m_word_diseq_todo_rel) {
            util::extract_symbols(word_equation.first, m_util_s, m, symbols_in_formula);
            util::extract_symbols(word_equation.second, m_util_s, m, symbols_in_formula);
        }

        for (const auto &word_equation: m_membership_todo_rel) {
            util::extract_symbols(std::get<1>(word_equation), m_util_s, m, symbols_in_formula);
        }

        /* Get number of dummy symbols needed for disequations and 'x not in RE' predicates.
         * We need some dummy symbols, to represent the symbols not occuring in predicates,
         * otherwise, we might return unsat even though the formula is sat. For example if
         * we had x != y and no other predicate, we would have no symbols and the formula
         * would be unsat. With one dummy symbol, it becomes sat.
         * We add new dummy symbols for each diseqation and 'x not in RE' predicate, as we
         * could be in situation where we have for example x != y, y != z, z != x, and
         * |x| = |y| = |z|. If we added only one dummy symbol, then this would be unsat,
         * but if we have three symbols, it becomes sat (which this formula is). We add
         * dummy symbols also for 'x not in RE' because they basically represent
         * disequations too (for example 'x not in "aaa"' and |x| = 3 should be sat, but
         * with only symbol "a" it becomes unsat).
         * 
         * FIXME: We can possibly create more dummy symbols than the size of alphabet
         * (from the string theory standard, the size of the alphabet is 196607), but
         * it is an edge-case that probably cannot happen.
         */
        size_t number_of_dummy_symbs = this->m_word_diseq_todo_rel.size();
        for (const auto& we : this->m_membership_todo_rel) {
            if(!std::get<2>(we)){
                number_of_dummy_symbs++;
            }
        }
        // to be safe, we set the minimum number of dummy symbols as 3
        number_of_dummy_symbs = std::max(number_of_dummy_symbs, size_t(3));

        // add needed number of dummy symbols to symbols_in_formula
        util::get_dummy_symbols(number_of_dummy_symbs, symbols_in_formula);

        /***************** END OF GATHERING SYMBOLS ******************/

        // satisfiability checking via universality checking
        // this heuristics is applied only to the case when there is a single regular constraint x notin RE.
        if(this->m_membership_todo_rel.size() == 1 && this->m_word_eq_todo_rel.size() == 0 && this->m_word_diseq_todo_rel.size() == 0 && this->len_vars.size() == 0) {
            const auto& reg_data = this->m_membership_todo_rel[0];
            if(!std::get<2>(reg_data)) {
                Nfa nfa{ util::conv_to_nfa(to_app(std::get<1>(reg_data)), m_util_s, m, symbols_in_formula, false, false) };
                Mata::Nfa::Nfa sigma_star;
                sigma_star.initial.insert(0);
                sigma_star.final.insert(0);
                for (const auto& symbol : symbols_in_formula) {
                    sigma_star.delta.add(0, symbol, 0);
                }

                if(Mata::Nfa::are_equivalent(nfa, sigma_star)) {
                    block_curr_len(expr_ref(this->m.mk_false(), this->m));
                    return FC_CONTINUE;
                } else {
                    return FC_DONE;
                }
            }
        }

        // Create automata assignment for the formula.
        AutAssignment aut_assignment{util::create_aut_assignment_for_formula(
                instance, m_membership_todo_rel, this->var_name, m_util_s, m, symbols_in_formula
        ) };

        expr_ref lengths(m);
        std::unordered_set<BasicTerm> init_length_sensitive_vars{ get_init_length_vars(aut_assignment) };


        // cache for storing already solved instances. For each instance we store the length formula obtained from the decision procedure.
        // if we get an instance that we have already solved, we use this stored length formula (if we run the procedure 
        // we get the same formula up to alpha reduction).
        if(m_params.m_loop_protect) {
            expr_ref refine = construct_refinement();
            if(refine != nullptr) {
                bool found = false;
                expr_ref len_formula(this->m);
                for(const auto& pr : this->axiomatized_instances) {
                    if(pr.first == refine) {
                        len_formula = pr.second;
                        found = true;
                        break;
                    }
                }
                if(found) {
                    block_curr_len(len_formula);
                    return FC_CONTINUE;
                }
            }
        }

        /// TODO: this needs to be finished
        /// Integrate the Nielsen transformation after the dispatcher is created!
        if(instance.is_quadratic() && false) {
            model_ref mod;
            NielsenDecisionProcedure nproc(instance, aut_assignment, 
                init_length_sensitive_vars, m, m_util_s, m_util_a, m_params);
            nproc.preprocess();
            expr_ref block_len(m.mk_false(), m);
            nproc.init_computation();
            while(nproc.compute_next_solution()) {
                lengths = nproc.get_lengths(this->var_name);
                if(check_len_sat(lengths, mod) == l_true) {
                    STRACE("str", tout << "len sat " << mk_pp(lengths, m) << std::endl;);
                    return FC_DONE;
                }
                block_len = m.mk_or(block_len, lengths);
                STRACE("str", tout << "len unsat" <<  mk_pp(lengths, m) << std::endl;);
            }
            if(!block_curr_len(block_len)) {
                return FC_CONTINUE;
            }
            return FC_CONTINUE;
        }
        

        // use underapproximation to solve
        if(m_params.m_underapproximation && solve_underapprox(instance, aut_assignment, init_length_sensitive_vars) == l_true) {
            STRACE("str", tout << "underapprox sat \n";);
            return FC_DONE;
        }

        DecisionProcedure dec_proc = DecisionProcedure{ instance, aut_assignment, 
            init_length_sensitive_vars, m, m_util_s, m_util_a, m_params };
        dec_proc.preprocess(PreprocessType::PLAIN, this->var_eqs.get_equivalence_bt());
        
        model_ref mod;
        if(init_length_sensitive_vars.size() > 0) {
            // check if the initial assignment is len unsat
            lengths = dec_proc.get_lengths(this->var_name);
            if(check_len_sat(lengths, mod) == l_false) {
                block_curr_len(lengths);
                return FC_DONE;
            }
        }

        expr_ref block_len(m.mk_false(), m);
        dec_proc.init_computation();
        while(dec_proc.compute_next_solution()) {
            lengths = dec_proc.get_lengths(this->var_name);
            if(dec_proc.get_init_length_vars().size() == 0 || check_len_sat(lengths, mod) == l_true) {
                STRACE("str", tout << "len sat " << mk_pp(lengths, m) << std::endl;);
                return FC_DONE;
            }
            if(dec_proc.get_init_length_vars().size() > 0) {
                block_len = m.mk_or(block_len, lengths);
            }
            STRACE("str", tout << "len unsat" <<  mk_pp(lengths, m) << std::endl;);
        }

        // all len solutions are unsat, we block the current assignment
        if(!block_curr_len(block_len)) {
            return FC_CONTINUE;
        }
        //block_curr_assignment();
        IN_CHECK_FINAL = false;
        TRACE("str", tout << "final_check ends\n";);
        return FC_CONTINUE;
    }

    /**
     * @brief Solve the given constraint using underapproximation.
     * 
     * @param instance Formula
     * @param aut_assignment Initial automata assignment
     * @param init_length_sensitive_vars Length sensitive variables
     * @return lbool SAT
     */
    lbool theory_str_noodler::solve_underapprox(const Formula& instance, const AutAssignment& aut_assignment, const std::unordered_set<BasicTerm>& init_length_sensitive_vars) {
        DecisionProcedure dec_proc = DecisionProcedure{ instance, aut_assignment, 
            init_length_sensitive_vars, m, m_util_s, m_util_a, m_params };
        dec_proc.preprocess(PreprocessType::UNDERAPPROX);
        
        expr_ref lengths(m);
        model_ref mod;
        dec_proc.init_computation();
        while(dec_proc.compute_next_solution()) {
            lengths = dec_proc.get_lengths(this->var_name);
            if(check_len_sat(lengths, mod) == l_true) {
                return l_true;
            }
        }
        return l_false;
    }

    model_value_proc *theory_str_noodler::mk_value(enode *const n, model_generator &mg) {
        app *const tgt = n->get_expr();
        (void) m;
        STRACE("str", tout << "mk_value: sort is " << mk_pp(tgt->get_sort(), m) << ", "
                           << mk_pp(tgt, m) << '\n';);
        return alloc(expr_wrapper_proc, tgt);
    }

    void theory_str_noodler::init_model(model_generator &mg) {
        STRACE("str", if (!IN_CHECK_FINAL) tout << "init_model\n";);
    }

    void theory_str_noodler::finalize_model(model_generator &mg) {
        STRACE("str", if (!IN_CHECK_FINAL) tout << "finalize_model\n";);
    }

    lbool theory_str_noodler::validate_unsat_core(expr_ref_vector &unsat_core) {
        return l_undef;
    }

    bool theory_str_noodler::is_of_this_theory(expr *const e) const {
        return is_app(e) && to_app(e)->get_family_id() == get_family_id();
    }

    bool theory_str_noodler::is_string_sort(expr *const e) const {
        return m_util_s.str.is_string_term(e);
    }

    bool theory_str_noodler::is_regex_sort(expr *const e) const {
        return m_util_s.is_re(e);
    }

    bool theory_str_noodler::is_const_fun(expr *const e) const {
        return is_app(e) && to_app(e)->get_decl()->get_arity() == 0;
    }

    bool theory_str_noodler::is_str_variable(const expr* expression) const {
        return util::is_str_variable(expression, m_util_s);
    }

    bool theory_str_noodler::is_variable(const expr* expression) const {
        return util::is_variable(expression, m_util_s);
    }

    expr_ref theory_str_noodler::mk_sub(expr *a, expr *b) {
        ast_manager &m = get_manager();

        expr_ref result(m_util_a.mk_sub(a, b), m);
        m_rewrite(result);
        return result;
    }

    literal theory_str_noodler::mk_literal(expr *const e) {
        ast_manager &m = get_manager();
        context &ctx = get_context();
        expr_ref ex{e, m};
        m_rewrite(ex);
        if (!ctx.e_internalized(ex)) {
            ctx.internalize(ex, false);
        }
        enode *const n = ctx.get_enode(ex);
        ctx.mark_as_relevant(n);
        return ctx.get_literal(ex);
    }

    bool_var theory_str_noodler::mk_bool_var(expr *const e) {
        ast_manager &m = get_manager();
        STRACE("str", tout << "mk_bool_var: " << mk_pp(e, m) << '\n';);
        if (!m.is_bool(e)) {
            return null_bool_var;
        }
        context &ctx = get_context();
        SASSERT(!ctx.b_internalized(e));
        const bool_var &bv = ctx.mk_bool_var(e);
        ctx.set_var_theory(bv, get_id());
        ctx.set_enode_flag(bv, true);
        return bv;
    }

    // probably deprecated
    // void theory_str_noodler::add_block_axiom(expr *const e) {
    //     STRACE("str", tout <<  __LINE__ << " " << __FUNCTION__ << mk_pp(e, get_manager()) << std::endl;);

    //     if (!axiomatized_terms.contains(e) || false) {
    //         axiomatized_terms.insert(e);
    //         if (e == nullptr || get_manager().is_true(e)) return;
    //         context &ctx = get_context();
    //         if (!ctx.b_internalized(e)) {
    //             ctx.internalize(e, false);
    //         }
    //         ctx.internalize(e, false);
    //         literal l{ctx.get_literal(e)};
    //         ctx.mk_th_axiom(get_id(), 1, &l);
    //         STRACE("str", ctx.display_literal_verbose(tout << "[Assert_e] block: \n", l) << '\n';);
    //     }
    // }

    void theory_str_noodler::add_axiom(expr *const e) {
        STRACE("str_axiom", tout << __LINE__ << " " << __FUNCTION__ << mk_pp(e, get_manager()) << std::endl;);


        if (!axiomatized_terms.contains(e)) {
            axiomatized_terms.insert(e);
            if (e == nullptr || get_manager().is_true(e)) return;
//        string_theory_propagation(e);
            context &ctx = get_context();
//        SASSERT(!ctx.b_internalized(e));
            if (!ctx.b_internalized(e)) {
                ctx.internalize(e, false);
            }
            ctx.internalize(e, false);
            literal l{ctx.get_literal(e)};
            ctx.mark_as_relevant(l);
            ctx.mk_th_axiom(get_id(), 1, &l);
            STRACE("str", ctx.display_literal_verbose(tout << "[Assert_e]\n", l) << '\n';);
        } else {
            //STRACE("str", tout << __LINE__ << " " << __FUNCTION__ << "already contains " << mk_pp(e, m) << std::endl;);
        }
    }

    void theory_str_noodler::add_axiom(std::initializer_list<literal> ls) {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);
        context &ctx = get_context();
        literal_vector lv;
        for (const auto &l : ls) {
            if (l != null_literal && l != false_literal) {
                ctx.mark_as_relevant(l);
                lv.push_back(l);
            }
        }
        ctx.mk_th_axiom(get_id(), lv, lv.size());
        STRACE("str_axiom", ctx.display_literals_verbose(tout << "[Assert_c]\n", lv) << '\n';);
        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);
    }

    /**
     * @brief Handle str.at(s,i)
     *
     * Translates to the following theory axioms:
     * 0 <= i < |s| -> s = xvy
     * 0 <= i < |s| -> v in re.allchar
     * 0 <= i < |s| -> |x| = i
     * i < 0 -> v = eps
     * i >= |s| -> v = eps
     *
     * We store
     * str.at(s,i) = v
     *
     * @param e str.at(s, i)
     */
    void theory_str_noodler::handle_char_at(expr *e) {
        STRACE("str", tout << "handle-charat: " << mk_pp(e, m) << '\n';);
        if (axiomatized_persist_terms.contains(e))
            return;

        axiomatized_persist_terms.insert(e);
        ast_manager &m = get_manager();
        expr *s = nullptr, *i = nullptr, *res = nullptr;
        VERIFY(m_util_s.str.is_at(e, s, i));

        expr_ref fresh = mk_str_var("at");
        expr_ref re(m_util_s.re.mk_in_re(fresh, m_util_s.re.mk_full_char(nullptr)), m);
        expr_ref zero(m_util_a.mk_int(0), m);
        literal i_ge_0 = mk_literal(m_util_a.mk_ge(i, zero));
        literal i_ge_len_s = mk_literal(m_util_a.mk_ge(mk_sub(i, m_util_s.str.mk_length(s)), zero));
        expr_ref emp(m_util_s.str.mk_empty(e->get_sort()), m);

        rational r;
        if(m_util_a.is_numeral(i, r)) {
            int val = r.get_int32();

            expr_ref y = mk_str_var("at_right");

            for(int j = val; j >= 0; j--) {
                y = m_util_s.str.mk_concat(m_util_s.str.mk_at(s, m_util_a.mk_int(j)), y);
            }
            string_theory_propagation(y);

            add_axiom({i_ge_len_s, mk_eq(s, y, false)});
            add_axiom({i_ge_len_s, mk_literal(re)});
            add_axiom({i_ge_len_s, mk_eq(m_util_a.mk_int(1), m_util_s.str.mk_length(fresh), false) });
            add_axiom({mk_eq(fresh, e, false)});
            add_axiom({i_ge_0, mk_eq(fresh, emp, false)});
            add_axiom({~i_ge_len_s, mk_eq(fresh, emp, false)});

            predicate_replace.insert(e, fresh.get());
            return;
        }
        if(util::is_len_sub(i, s, m, m_util_s, m_util_a, res) && m_util_a.is_numeral(res, r)) {
            int val = r.get_int32();

            expr_ref y = mk_str_var("at_left");

            for(int j = val; j < 0; j++) {
                y = m_util_s.str.mk_concat(y, m_util_s.str.mk_at(s, m_util_a.mk_add(m_util_a.mk_int(j), m_util_s.str.mk_length(s))));
            }
            string_theory_propagation(y);

            add_axiom({~i_ge_0, mk_eq(s, y, false)});
            add_axiom({~i_ge_0, mk_eq(m_util_a.mk_int(1), m_util_s.str.mk_length(fresh), false) });
            add_axiom({mk_eq(fresh, e, false)});
            add_axiom({~i_ge_0, mk_literal(re)});
            add_axiom({i_ge_0, mk_eq(fresh, emp, false)});

            predicate_replace.insert(e, fresh.get());
            return;
        }

        expr_ref one(m_util_a.mk_int(1), m);
        expr_ref x = mk_str_var("at_left");
        expr_ref y = mk_str_var("at_right");
        expr_ref xey(m_util_s.str.mk_concat(x, m_util_s.str.mk_concat(fresh, y)), m);
        string_theory_propagation(xey);

        expr_ref len_x(m_util_s.str.mk_length(x), m);
 
        add_axiom({~i_ge_0, i_ge_len_s, mk_eq(s, xey, false)});
        add_axiom({~i_ge_0, i_ge_len_s, mk_eq(one, m_util_s.str.mk_length(fresh), false)});
        add_axiom({~i_ge_0, i_ge_len_s, mk_literal(re)});
        add_axiom({~i_ge_0, i_ge_len_s, mk_eq(i, len_x, false)});
        add_axiom({i_ge_0, mk_eq(fresh, emp, false)});
        add_axiom({~i_ge_len_s, mk_eq(fresh, emp, false)});
        add_axiom({mk_eq(fresh, e, false)});

        // add the replacement charat -> v
        predicate_replace.insert(e, fresh.get());
        // update length variables
        util::get_str_variables(s, this->m_util_s, m, this->len_vars);
        this->len_vars.insert(x);
    }

    void theory_str_noodler::handle_substr_int(expr *e) {
        expr *s = nullptr, *i = nullptr, *l = nullptr;
        VERIFY(m_util_s.str.is_extract(e, s, i, l));

        rational r;
        if(!m_util_a.is_numeral(i, r)) {
            return;
        }

        expr_ref ls(m_util_s.str.mk_length(s), m);
        expr_ref ls_minus_i_l(mk_sub(mk_sub(ls, i), l), m);
        expr_ref zero(m_util_a.mk_int(0), m);
        expr_ref eps(m_util_s.str.mk_string(""), m);

        literal i_ge_0 = mk_literal(m_util_a.mk_ge(i, zero));
        literal ls_le_i = mk_literal(m_util_a.mk_le(mk_sub(i, ls), zero));
        literal li_ge_ls = mk_literal(m_util_a.mk_ge(ls_minus_i_l, zero));
        literal l_ge_zero = mk_literal(m_util_a.mk_ge(l, zero));
        literal ls_le_0 = mk_literal(m_util_a.mk_le(ls, zero));

        expr_ref x(m_util_s.str.mk_string(""), m);
        expr_ref v = mk_str_var("substr");

        int val = r.get_int32();
        for(int i = 0; i < val; i++) {
            expr_ref var = mk_str_var("pre_substr");
            expr_ref re(m_util_s.re.mk_in_re(var, m_util_s.re.mk_full_char(nullptr)), m);
            x = m_util_s.str.mk_concat(x, var);
            add_axiom({~i_ge_0, ~ls_le_i, mk_literal(re)});
        }

        expr_ref le(m_util_s.str.mk_length(v), m);
        expr_ref y = mk_str_var("post_substr");
        expr_ref xe(m_util_s.str.mk_concat(x, v), m);
        expr_ref xey(m_util_s.str.mk_concat(x, v, y), m);
        
        rational rl;
        expr * num_len;
        if(m_util_a.is_numeral(l, rl)) {
            int lval = rl.get_int32();
            expr_ref substr_re(m);
            for(int i = 0; i < lval; i++) {
                if(substr_re == nullptr) {
                    substr_re = m_util_s.re.mk_full_char(nullptr);
                } else {
                    substr_re = m_util_s.re.mk_concat(substr_re, m_util_s.re.mk_full_char(nullptr));
                }  
            }
            expr_ref substr_in(m_util_s.re.mk_in_re(v, substr_re), m);

            // 0 <= i <= |s| && 0 <= l <= |s| - i -> |v| in substr_re
            add_axiom({~i_ge_0, ~ls_le_i, ~l_ge_zero, ~li_ge_ls, mk_eq(le, l, false)});
            add_axiom({~i_ge_0, ~ls_le_i, ~l_ge_zero, ~li_ge_ls, mk_literal(substr_in)});
            // 0 <= i <= |s| && |s| < l + i  -> s = x.v
            add_axiom({~i_ge_0, ~ls_le_i, li_ge_ls, mk_eq(xe, s, false)});
        } else if(util::is_len_sub(l, s, m, m_util_s, m_util_a, num_len) && m_util_a.is_numeral(num_len, rl) && rl == r) {
            xe = expr_ref(m_util_s.str.mk_concat(x, v), m);
            xey = expr_ref(m_util_s.str.mk_concat(x, v), m);
        } else {
            // 0 <= i <= |s| && 0 <= l <= |s| - i -> |v| = l
             add_axiom({~i_ge_0, ~ls_le_i, ~l_ge_zero, ~li_ge_ls, mk_eq(le, l, false)});
             // 0 <= i <= |s| && |s| < l + i  -> |v| = |s| - i
             add_axiom({~i_ge_0, ~ls_le_i, li_ge_ls, mk_eq(le, mk_sub(ls, i), false)});
             this->len_vars.insert(v);
        }

        string_theory_propagation(xe);
        string_theory_propagation(xey);
        // 0 <= i <= |s| -> xvy = s
        add_axiom({~i_ge_0, ~ls_le_i, mk_eq(xey, s, false)});
        // 0 <= i <= |s| && 0 <= l <= |s| - i -> |v| = l
        add_axiom({~i_ge_0, ~ls_le_i, ~l_ge_zero, ~li_ge_ls, mk_eq(le, l, false)});
        // 0 <= i <= |s| && l < 0 -> v = eps
        add_axiom({~i_ge_0, ~ls_le_i, l_ge_zero, mk_eq(v, eps, false)});
        // i < 0 -> v = eps
        add_axiom({i_ge_0, mk_eq(v, eps, false)});
        // not(0 <= l <= |s| - i) -> v = eps
        add_axiom({ls_le_i, mk_eq(v, eps, false)});
        // i > |s| -> v = eps
        add_axiom({~ls_le_0, mk_eq(v, eps, false)});
        // substr(s, i, n) = v
        add_axiom({mk_eq(v, e, false)});

        // add the replacement substr -> v
        this->predicate_replace.insert(e, v.get());
        // update length variables
        util::get_str_variables(s, this->m_util_s, m, this->len_vars);
        // add length |v| = l. This is not true entirely, because there could be a case that v = eps. 
        // but this case is handled by epsilon propagation preprocessing (this variable will not in the system
        // after that)
        this->var_eqs.add(expr_ref(l, m), v);
    }

    /**
     * @brief Handle str.substr(s,i,l)
     *
     * Translates to the following theory axioms:
     * 0 <= i <= |s| -> x.v.y = s
     * 0 <= i <= |s| -> |x| = i
     * 0 <= i <= |s| && 0 <= l <= |s| - i -> |v| = l
     * 0 <= i <= |s| && |s| < l + i  -> |v| = |s| - i
     * 0 <= i <= |s| && l < 0 -> v = eps
     * i < 0 -> v = eps
     * not(0 <= l <= |s| - i) -> v = eps
     * i > |s| -> v = eps
     *
     * We store
     * substr(s, i, n) = v
     *
     * @param e str.substr(s, i, l)
     */
    void theory_str_noodler::handle_substr(expr *e) {
        STRACE("str", tout << "handle-substr: " << mk_pp(e, m) << '\n';);
        if (axiomatized_persist_terms.contains(e))
            return;

        axiomatized_persist_terms.insert(e);

        ast_manager &m = get_manager();
        expr *s = nullptr, *i = nullptr, *l = nullptr;
        VERIFY(m_util_s.str.is_extract(e, s, i, l));

        expr* num = nullptr;
        expr* pred = nullptr;
        rational r;
        if(m_util_a.is_numeral(i)) {
            handle_substr_int(e);
            return;
        }

        expr_ref post_bound(m_util_a.mk_ge(m_util_a.mk_add(i, l), m_util_s.str.mk_length(s)), m);
        m_rewrite(post_bound); // simplify

        expr_ref v = mk_str_var("substr");
        expr_ref xvar = mk_str_var("pre_substr");
        expr_ref x = xvar;
        expr_ref y = mk_str_var("post_substr");

        // if i + l >= |s|, we can set post_substr to eps
        if(m.is_true(post_bound)) {
            y = expr_ref(m_util_s.str.mk_string(""), m);
        }
        std::vector<expr_ref> vars;
        // if i is of the form i = n + ...., create pre_substr . in_substr1 ... in_substrn to be x
        if(m_util_a.is_add(i, num, pred) && m_util_a.is_numeral(num, r)) {
            for(int i = 0; i < r.get_int32(); i++) {
                expr_ref fv = mk_str_var("in_substr");
                x = m_util_s.str.mk_concat(x, fv);
                vars.push_back(fv);
            }    
        }
        expr_ref xe(m_util_s.str.mk_concat(x, v), m);
        expr_ref xey(m_util_s.str.mk_concat(x, v, y), m);

       
        expr_ref ls(m_util_s.str.mk_length(s), m);
        expr_ref lx(m_util_s.str.mk_length(x), m);
        expr_ref le(m_util_s.str.mk_length(v), m);
        expr_ref ls_minus_i_l(mk_sub(mk_sub(ls, i), l), m);

        expr_ref zero(m_util_a.mk_int(0), m);
        expr_ref eps(m_util_s.str.mk_string(""), m);

        literal i_ge_0 = mk_literal(m_util_a.mk_ge(i, zero));
        literal ls_le_i = mk_literal(m_util_a.mk_le(mk_sub(i, ls), zero));
        literal li_ge_ls = mk_literal(m_util_a.mk_ge(ls_minus_i_l, zero));
        literal l_ge_zero = mk_literal(m_util_a.mk_ge(l, zero));
        literal ls_le_0 = mk_literal(m_util_a.mk_le(ls, zero));

        string_theory_propagation(xe);
        string_theory_propagation(xey);

        // create axioms in_substri is Sigma
        for(const expr_ref& val : vars) {
            expr_ref re(m_util_s.re.mk_in_re(val, m_util_s.re.mk_full_char(nullptr)), m);
            add_axiom({~i_ge_0, ~ls_le_i, mk_literal(re)});
        }
        // 0 <= i <= |s| -> xvy = s
        add_axiom({~i_ge_0, ~ls_le_i, mk_eq(xey, s, false)});
        // 0 <= i <= |s| -> |x| = i
        add_axiom({~i_ge_0, ~ls_le_i, mk_eq(lx, i, false)});
        // 0 <= i <= |s| && 0 <= l <= |s| - i -> |v| = l
        add_axiom({~i_ge_0, ~ls_le_i, ~l_ge_zero, ~li_ge_ls, mk_eq(le, l, false)});
        // 0 <= i <= |s| && |s| < l + i  -> |v| = |s| - i
        add_axiom({~i_ge_0, ~ls_le_i, li_ge_ls, mk_eq(le, mk_sub(ls, i), false)});
        // 0 <= i <= |s| && l < 0 -> v = eps
        add_axiom({~i_ge_0, ~ls_le_i, l_ge_zero, mk_eq(v, eps, false)});
        // i < 0 -> v = eps
        add_axiom({i_ge_0, mk_eq(v, eps, false)});
        // not(0 <= l <= |s| - i) -> v = eps
        add_axiom({ls_le_i, mk_eq(v, eps, false)});
        // i > |s| -> v = eps
        add_axiom({~ls_le_0, mk_eq(v, eps, false)});
        // substr(s, i, n) = v
        add_axiom({mk_eq(v, e, false)});

        // add the replacement substr -> v
        this->predicate_replace.insert(e, v.get());
        // update length variables
        util::get_str_variables(s, this->m_util_s, m, this->len_vars);
        this->len_vars.insert(v);
        if(vars.size() > 0) {
            this->var_eqs.add(expr_ref(pred, m), xvar);
            this->len_vars.insert(xvar);
        } else {
            this->var_eqs.add(expr_ref(i, m), x);
            this->len_vars.insert(x);
        }        
    }

    /**
     * @brief Handling of str.replace(a,s,t) = v ... a where to replace, s what to find, t replacement.
     * Translates to the following theory axioms:
     * replace(a,s,t) = v
     * a = eps && s != eps -> v = a
     * (not(contains(a,s))) -> v = a
     * s = eps -> v = t.a
     * contains(a,s) && a != eps && s != eps -> a = x.s.y
     * contains(a,s) && a != eps && s != eps -> v = x.t.y
     * tighttestprefix(s,t)
     *
     * @param r replace term
     */
    void theory_str_noodler::handle_replace(expr *r) {
        STRACE("str", tout << "handle-replace: " << mk_pp(r, m) << '\n';);

        if(axiomatized_persist_terms.contains(r))
            return;

        axiomatized_persist_terms.insert(r);
        context& ctx = get_context();
        expr* a = nullptr, *s = nullptr, *t = nullptr;
        VERIFY(m_util_s.str.is_replace(r, a, s, t));
        expr_ref v = mk_str_var("replace");
        expr_ref x = mk_str_var("replace_left");
        expr_ref y = mk_str_var("replace_right");
        expr_ref xty = mk_concat(x, mk_concat(t, y));
        expr_ref xsy = mk_concat(x, mk_concat(s, y));
        literal a_emp = mk_eq_empty(a, true);
        literal s_emp = mk_eq_empty(s, true);
        literal cnt = mk_literal(m_util_s.str.mk_contains(a, s));

        // replace(a,s,t) = v
        add_axiom({mk_eq(v, r, false)});
        // a = eps && s != eps -> v = a
        add_axiom({~a_emp, s_emp, mk_eq(v, a, false)});
        // (not(contains(a,s))) -> v = a
        add_axiom({cnt, mk_eq(v, a, false)});
        // s = eps -> v = t.a
        add_axiom({~s_emp, mk_eq(v, mk_concat(t, a),false)});
        // contains(a,s) && a != eps && s != eps -> a = x.s.y
        add_axiom({~cnt, a_emp, s_emp, mk_eq(a, xsy,false)});
        // contains(a,s) && a != eps && s != eps -> v = x.t.y
        add_axiom({~cnt, a_emp, s_emp, mk_eq(v, xty,false)});
        ctx.force_phase(cnt);
        // tighttestprefix(s,t)
        tightest_prefix(s, x);

        predicate_replace.insert(r, v.get());
    }

    /**
     * @brief Handling of str.indexof(t, s, offset) = indexof
     * Translates to the following theory axioms:
     * The case of offset = 0
     * not(contains(t,s)) -> indexof = -1
     * t = eps && s != eps -> indexof = -1
     * s = eps -> indexof = 0
     * contains(t, s) && s != eps -> t = x.s.y
     * contains(t, s) && s != eps -> indexof = |x|
     * contains(t, s) -> indexof >= 0
     * tightestprefix(s,x)
     *
     * The case of offset > 0
     * not(contains(t,s)) -> indexof = -1
     * t = eps && s != eps -> indexof = -1
     * offset >= |t| && s != eps -> indexof = -1
     * offset > |t| -> indexof = -1
     * offset >= |t| && offset <= |t| && s = eps -> indexof = offset
     * offset >= 0 && offset < |t| -> t = xy
     * offset >= 0 && offset < |t| -> |x| = offset
     * offset >= 0 && offset < |t| && indexof(y,s,0) = -1 -> indexof = -1
     * offset >= 0 && offset < |t| && indexof(y,s,0) >= 0 -> offset + indexof(y,s,0) = indexof
     * offset < 0 -> indexof = -1
     *
     * @param i indexof term
     */
    void theory_str_noodler::handle_index_of(expr *i) {
        STRACE("str", tout << "handle-indexof: " << mk_pp(i, m) << '\n';);
        if(axiomatized_persist_terms.contains(i))
            return;

        axiomatized_persist_terms.insert(i);
        ast_manager &m = get_manager();
        expr *s = nullptr, *t = nullptr, *offset = nullptr;
        rational r;
        VERIFY(m_util_s.str.is_index(i, t, s) || m_util_s.str.is_index(i, t, s, offset));

        expr_ref minus_one(m_util_a.mk_int(-1), m);
        expr_ref zero(m_util_a.mk_int(0), m);
        expr_ref emp(m_util_s.str.mk_empty(t->get_sort()), m);
        literal cnt = mk_literal(m_util_s.str.mk_contains(t, s));

        literal i_eq_m1 = mk_eq(i, minus_one, false);
        literal i_eq_0 = mk_eq(i, zero, false);
        literal s_eq_empty = mk_eq(s, emp, false);
        literal t_eq_empty = mk_eq(t, emp, false);

        // not(contains(t,s)) -> indexof = -1
        add_axiom({cnt, i_eq_m1});
        // t = eps && s != eps -> indexof = -1
        add_axiom({~t_eq_empty, s_eq_empty, i_eq_m1});

        if (!offset || (m_util_a.is_numeral(offset, r) && r.is_zero())) {
            expr_ref x = mk_str_var("index_left");
            expr_ref y = mk_str_var("index_right");
            expr_ref xsy(m_util_s.str.mk_concat(x, s, y), m);
            string_theory_propagation(xsy);

            expr_ref lenx(m_util_s.str.mk_length(x), m);
            // s = eps -> indexof = 0
            add_axiom({~s_eq_empty, i_eq_0});
            // contains(t, s) && s != eps -> t = x.s.y
            add_axiom({~cnt, s_eq_empty, mk_eq(t, xsy, false)});
            // contains(t, s) && s != eps -> indexof = |x|
            add_axiom({~cnt, s_eq_empty, mk_eq(i, lenx, false)});
            // contains(t, s) -> indexof >= 0
            add_axiom({~cnt, mk_literal(m_util_a.mk_ge(i, zero))});
            tightest_prefix(s, x);

            // update length variables
            this->len_vars.insert(x);
            this->var_eqs.add(expr_ref(i, m), x);

        } else {
            expr_ref len_t(m_util_s.str.mk_length(t), m);
            literal offset_ge_len = mk_literal(m_util_a.mk_ge(mk_sub(offset, len_t), zero));
            literal offset_le_len = mk_literal(m_util_a.mk_le(mk_sub(offset, len_t), zero));
            literal i_eq_offset = mk_eq(i, offset, false);
            // offset >= |t| && s != eps -> indexof = -1
            add_axiom({~offset_ge_len, s_eq_empty, i_eq_m1});
            // offset > |t| -> indexof = -1
            add_axiom({offset_le_len, i_eq_m1});
            // offset >= |t| && offset <= |t| && s = eps -> indexof = offset
            add_axiom({~offset_ge_len, ~offset_le_len, ~s_eq_empty, i_eq_offset});

            expr_ref x = mk_str_var("index_left_off");
            expr_ref y = mk_str_var("index_right_off");
            expr_ref xy(m_util_s.str.mk_concat(x, y), m);
            string_theory_propagation(xy);

            expr_ref indexof0(m_util_s.str.mk_index(y, s, zero), m);
            expr_ref offset_p_indexof0(m_util_a.mk_add(offset, indexof0), m);
            literal offset_ge_0 = mk_literal(m_util_a.mk_ge(offset, zero));
            // offset >= 0 && offset < |t| -> t = xy
            add_axiom({~offset_ge_0, offset_ge_len, mk_eq(t, xy, false)});
            // offset >= 0 && offset < |t| -> |x| = offset
            add_axiom({~offset_ge_0, offset_ge_len, mk_eq(m_util_s.str.mk_length(x), offset, false)});
            // offset >= 0 && offset < |t| && indexof(y,s,0) = -1 -> indexof = -1
            add_axiom({~offset_ge_0, offset_ge_len, ~mk_eq(indexof0, minus_one, false), i_eq_m1});
            // offset >= 0 && offset < |t| && indexof(y,s,0) >= 0 -> offset + indexof(y,s,0) = indexof
            add_axiom({~offset_ge_0, offset_ge_len, ~mk_literal(m_util_a.mk_ge(indexof0, zero)),
                            mk_eq(offset_p_indexof0, i, false)});
            // offset < 0 -> indexof = -1
            add_axiom({offset_ge_0, i_eq_m1});

            // update length variables
            util::get_str_variables(t, this->m_util_s, m, this->len_vars);
            this->len_vars.insert(x);
        }
    }

    /**
     * @brief String term @p x does not contain @p s as a substring.
     * Translates to the following theory axioms:
     * not(s = eps) -> s = s1.s2 where s1 = s[0,-2], s2 = s[-1] in the case of @p s being a string
     * not(s = eps) -> not(contains(x.s1, s))
     * not(s = eps) -> s2 in re.allchar (is a single character)
     *
     * @param s Substring that should not be present
     * @param x String term
     */
    void theory_str_noodler::tightest_prefix(expr* s, expr* x) {
        expr_ref s1 = mk_first(s);
        expr_ref c  = mk_last(s);
        expr_ref s1c = mk_concat(s1, c);
        literal s_eq_emp = mk_eq_empty(s);
        expr_ref re(m_util_s.re.mk_in_re(c, m_util_s.re.mk_full_char(nullptr)), m);

        add_axiom({s_eq_emp, mk_literal(re) });
        add_axiom({s_eq_emp, mk_literal(m.mk_eq(s, s1c))});
        add_axiom({s_eq_emp, ~mk_literal(m_util_s.str.mk_contains(mk_concat(x, s1), s))});
    }

    /**
     * @brief Get string term representing first |s| - 1 characters of @p s.
     *
     * @param s String term
     * @return expr_ref String term
     */
    expr_ref theory_str_noodler::mk_first(expr* s) {
        zstring str;
        if (m_util_s.str.is_string(s, str) && str.length() > 0) {
            return expr_ref(m_util_s.str.mk_string(str.extract(0, str.length()-1)), m);
        }
        return mk_str_var("index_first");
    }

    /**
     * @brief Get string term representing last character of @p s.
     *
     * @param s String term
     * @return expr_ref String term
     */
    expr_ref theory_str_noodler::mk_last(expr* s) {
        zstring str;
        if (m_util_s.str.is_string(s, str) && str.length() > 0) {
            return expr_ref(m_util_s.str.mk_string(str.extract(str.length()-1, 1)), m);
        }
        return mk_str_var("index_last");
    }

    expr_ref theory_str_noodler::mk_concat(expr* e1, expr* e2) {
        return expr_ref(m_util_s.str.mk_concat(e1, e2), m);
    }

    literal theory_str_noodler::mk_eq_empty(expr* _e, bool phase) {
        context& ctx = get_context();
        expr_ref e(_e, m);
        SASSERT(m_util_s.is_seq(e));
        expr_ref emp(m);
        zstring s;
        if (m_util_s.str.is_empty(e)) {
            return true_literal;
        }
        expr_ref_vector concats(m);
        m_util_s.str.get_concat(e, concats);
        for (auto c : concats) {
            if (m_util_s.str.is_unit(c)) {
                return false_literal;
            }
            if (m_util_s.str.is_string(c, s) && s.length() > 0) {
                return false_literal;
            }
        }
        emp = m_util_s.str.mk_empty(e->get_sort());

        literal lit = mk_eq(e, emp, false);
        ctx.force_phase(phase?lit:~lit);
        ctx.mark_as_relevant(lit);
        return lit;
    }


    /**
     * @brief Handling of str.prefix(x, y) = e (x is a prefix of y)
     * Translates to the following theory axioms:
     * e -> y = x.v
     *
     * @param e prefix term
     */
    void theory_str_noodler::handle_prefix(expr *e) {
        if(axiomatized_persist_terms.contains(e))
            return;

        axiomatized_persist_terms.insert(e);
        ast_manager &m = get_manager();
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_prefix(e, x, y));

        expr_ref fresh = mk_str_var("prefix");
        expr_ref xs(m_util_s.str.mk_concat(x, fresh), m);
        string_theory_propagation(xs);
        literal not_e = mk_literal(e);
        add_axiom({~not_e, mk_eq(y, xs, false)});
    }

    /**
     * @brief Handle not(prefix(x,y)); prefix(x,y) = @p e
     * Translates to the following theory axioms:
     * not(e) && |x| <= |y| -> x = p.mx.qx
     * not(e) && |x| <= |y| -> y = p.my.qy
     * not(e) && |x| <= |y| -> mx in re.allchar
     * not(e) && |x| <= |y| -> my in re.allchar
     * not(e) && |x| <= |y| -> mx != my
     *
     * @param e prefix term
     */
    void theory_str_noodler::handle_not_prefix(expr *e) {
        if(axiomatized_persist_terms.contains(m.mk_not(e)))
            return;

        axiomatized_persist_terms.insert(m.mk_not(e));
        ast_manager &m = get_manager();
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_prefix(e, x, y));

        expr_ref p = mk_str_var("nprefix_left");
        expr_ref mx = mk_str_var("nprefix_midx");
        expr_ref my = mk_str_var("nprefix_midy");
        expr_ref qx = mk_str_var("nprefix_rightx");
        expr_ref qy = mk_str_var("nprefix_righty");

        expr_ref len_x_gt_len_y{m_util_a.mk_gt(m_util_a.mk_sub(m_util_s.str.mk_length(x),m_util_s.str.mk_length(y)), m_util_a.mk_int(0)),m};
        literal len_y_gt_len_x = mk_literal(len_x_gt_len_y);
        expr_ref pmx(m_util_s.str.mk_concat(p, mx), m);
        string_theory_propagation(pmx);
        expr_ref pmxqx(m_util_s.str.mk_concat(pmx, qx), m);
        string_theory_propagation(pmxqx);
        expr_ref pmy(m_util_s.str.mk_concat(p, my), m);
        string_theory_propagation(pmy);
        expr_ref pmyqy(m_util_s.str.mk_concat(pmy, qy), m);
        string_theory_propagation(pmyqy);

        literal x_eq_pmq = mk_eq(x,pmxqx,false);
        literal y_eq_pmq = mk_eq(y,pmyqy,false);
        literal eq_mx_my = mk_literal(m.mk_not(ctx.mk_eq_atom(mx,my)));

        expr_ref rex(m_util_s.re.mk_in_re(mx, m_util_s.re.mk_full_char(nullptr)), m);
        expr_ref rey(m_util_s.re.mk_in_re(my, m_util_s.re.mk_full_char(nullptr)), m);

        literal lit_e = mk_literal(e);
        // not(e) && |x| <= |y| -> x = p.mx.qx
        add_axiom({lit_e, len_y_gt_len_x, x_eq_pmq});
        // not(e) && |x| <= |y| -> y = p.my.qy
        add_axiom({lit_e, len_y_gt_len_x, y_eq_pmq});
        // not(e) && |x| <= |y| -> mx in re.allchar
        add_axiom({lit_e, len_y_gt_len_x, mk_literal(rex)});
        // not(e) && |x| <= |y| -> my in re.allchar
        add_axiom({lit_e, len_y_gt_len_x, mk_literal(rey)});
        // not(e) && |x| <= |y| -> mx != my
        add_axiom({lit_e, len_y_gt_len_x, eq_mx_my});

        // update length variables
        util::get_str_variables(x, this->m_util_s, m, this->len_vars);
        util::get_str_variables(y, this->m_util_s, m, this->len_vars);
    }

    /**
     * @brief Handling of str.suffix(x, y) = e (x is a suffix of y)
     * Translates to the following theory axioms:
     * e -> y = v.x
     *
     * @param e suffix term
     */
    void theory_str_noodler::handle_suffix(expr *e) {
        if(axiomatized_persist_terms.contains(e))
            return;

        axiomatized_persist_terms.insert(e);
        ast_manager &m = get_manager();
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_suffix(e, x, y));

        expr_ref fresh = mk_str_var("suffix");
        expr_ref px(m_util_s.str.mk_concat(fresh, x), m);
        string_theory_propagation(px);
        literal not_e = mk_literal(e);
        add_axiom({~not_e, mk_eq(y, px, false)});
    }

    /**
     * @brief Handle not(suffix(x,y)); suffix(x,y) = @p e
     * Translates to the following theory axioms:
     * not(e) && |x| <= |y| -> x = px.mx.q
     * not(e) && |x| <= |y| -> y = py.my.q
     * not(e) && |x| <= |y| -> mx in re.allchar
     * not(e) && |x| <= |y| -> my in re.allchar
     * not(e) && |x| <= |y| -> mx != my
     *
     * @param e prefix term
     */
    void theory_str_noodler::handle_not_suffix(expr *e) {
        if(axiomatized_persist_terms.contains(m.mk_not(e)))
            return;

        axiomatized_persist_terms.insert(m.mk_not(e));
        ast_manager &m = get_manager();
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_suffix(e, x, y));

        expr_ref q = mk_str_var("nsuffix_right");
        expr_ref mx = mk_str_var("nsuffix_midx");
        expr_ref my = mk_str_var("nsuffix_midy");
        expr_ref px = mk_str_var("nsuffix_leftx");
        expr_ref py = mk_str_var("nsuffix_lefty");

        expr_ref len_x_gt_len_y{m_util_a.mk_gt(m_util_a.mk_sub(m_util_s.str.mk_length(x),m_util_s.str.mk_length(y)), m_util_a.mk_int(0)),m};
        literal len_y_gt_len_x = mk_literal(len_x_gt_len_y);
        expr_ref pxmx(m_util_s.str.mk_concat(px, mx), m);
        string_theory_propagation(pxmx);
        expr_ref pxmxq(m_util_s.str.mk_concat(pxmx, q), m);
        string_theory_propagation(pxmxq);
        expr_ref pymy(m_util_s.str.mk_concat(py, my), m);
        string_theory_propagation(pymy);
        expr_ref pymyq(m_util_s.str.mk_concat(pymy, q), m);
        string_theory_propagation(pymyq);

        literal x_eq_pmq = mk_eq(x,pxmxq,false);
        literal y_eq_pmq = mk_eq(y,pymyq,false);
        literal eq_mx_my = mk_literal(m.mk_not(ctx.mk_eq_atom(mx,my)));
        literal lit_e = mk_literal(e);

        expr_ref rex(m_util_s.re.mk_in_re(mx, m_util_s.re.mk_full_char(nullptr)), m);
        expr_ref rey(m_util_s.re.mk_in_re(my, m_util_s.re.mk_full_char(nullptr)), m);

        // not(e) && |x| <= |y| -> x = px.mx.q
        add_axiom({lit_e, len_y_gt_len_x, x_eq_pmq});
        // not(e) && |x| <= |y| -> y = py.my.q
        add_axiom({lit_e, len_y_gt_len_x, y_eq_pmq});
        // not(e) && |x| <= |y| -> mx in re.allchar
        add_axiom({lit_e, len_y_gt_len_x, mk_literal(rex)});
        // not(e) && |x| <= |y| -> my in re.allchar
        add_axiom({lit_e, len_y_gt_len_x, mk_literal(rey)});
        // not(e) && |x| <= |y| -> mx != my
        add_axiom({lit_e, len_y_gt_len_x, eq_mx_my});

        // update length variables
        util::get_str_variables(x, this->m_util_s, m, this->len_vars);
        util::get_str_variables(y, this->m_util_s, m, this->len_vars);
    }

    /**
     * @brief Handle contains
     * Translates to the following theory axioms:
     * str.contains(x,y) -> x = pys
     *
     * @param e str.contains(x,y)
     */
    void theory_str_noodler::handle_contains(expr *e) {
        if(axiomatized_persist_terms.contains(e))
            return;

        axiomatized_persist_terms.insert(e);
        STRACE("str", tout  << "handle contains " << mk_pp(e, m) << std::endl;);
        ast_manager &m = get_manager();
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_contains(e, x, y));
        expr_ref p = mk_str_var("contains_left");
        expr_ref s = mk_str_var("contains_right");
        expr_ref pys(m_util_s.str.mk_concat(m_util_s.str.mk_concat(p, y), s), m);

        string_theory_propagation(pys);
        literal not_e = mk_literal(mk_not({e, m}));
        add_axiom({not_e, mk_eq(x, pys, false)});
    }


    /**
     * @brief Heuristics for handling not contains: not(contains(s, t)).
     * So far only the case when t is a string literal is implemented.
     *
     * @param e contains term.
     */
    void theory_str_noodler::handle_not_contains(expr *e) {
        expr* cont = this->m.mk_not(e);
        expr *x = nullptr, *y = nullptr;
        VERIFY(m_util_s.str.is_contains(e, x, y));

        STRACE("str", tout  << "handle not(contains) " << mk_pp(e, m) << std::endl;);
        zstring s;
        if(m_util_s.str.is_string(y, s)) {
            // if(axiomatized_persist_terms.contains(cont))
            //     return;
            // axiomatized_persist_terms.insert(cont);

            expr_ref re(m_util_s.re.mk_in_re(x, m_util_s.re.mk_concat(m_util_s.re.mk_star(m_util_s.re.mk_full_char(nullptr)),
                m_util_s.re.mk_concat(m_util_s.re.mk_to_re(m_util_s.str.mk_string(s)),
                m_util_s.re.mk_star(m_util_s.re.mk_full_char(nullptr)))) ), m);
          
            add_axiom({mk_literal(e), ~mk_literal(re)});
        } else {
            // TODO: shouldn't we just throw error here that we cannot handle not contains? or are we planning to handle it? or maybe it might be irrelevant, but in final_check_eh we throw unknown anyway, we do not check for rellevancy
            m_not_contains_todo.push_back({{x, m},{y, m}});
        }
    }

    void theory_str_noodler::handle_in_re(expr *const e, const bool is_true) {
        expr *s = nullptr, *re = nullptr;
        VERIFY(m_util_s.str.is_in_re(e, s, re));
        ast_manager& m = get_manager();
        STRACE("str", tout  << "handle_in_re " << mk_pp(e, m) << " " << is_true << std::endl;);

        app_ref re_constr(to_app(s), m);
        expr_ref re_atom(e, m);
        /// Check if @p re_constr is a simple variable. If not (it is, e.g., concatenation of string terms),
        /// this complex term T is replaced by a fresh variable X. The following axioms are hence added: X = T && X in RE.
        if(re_constr->get_num_args() != 0) {
            expr_ref var(m);
            if(this->predicate_replace.contains(re_constr)) {
                var = expr_ref(this->predicate_replace[re_constr], m);
            } else {
                var = mk_str_var("revar");
                this->predicate_replace.insert(re_constr.get(), var.get());
            }
            
            // app_ref fv(this->m_util_s.mk_skolem(this->m.mk_fresh_var_name(), 0, nullptr, this->m_util_s.mk_string_sort()), m);
            expr_ref eq_fv(mk_eq_atom(var.get(), s), m);
            expr_ref n_re(this->m_util_s.re.mk_in_re(var, re), m);
            expr_ref re_orig(e, m);

            // propagate_basic_string_axioms(ctx.get_enode(eq_fv));
            
            if(!is_true) {
                n_re = m.mk_not(n_re);
                re_orig = m.mk_not(re_orig);
            }
            add_axiom({mk_literal(eq_fv)});
            add_axiom({~mk_literal(re_orig), mk_literal(n_re)});
            
            re_constr = to_app(var); 
            re_atom = n_re;
        } 

        // generate length formula for the regular expression
        // if(ctx.is_relevant(e) && is_true && false) { // turn it off for now
        //     std::set<uint32_t> alphabet;
        //     util::extract_symbols(re, this->m_util_s, this->m, alphabet);
        //     util::get_dummy_symbols(2, alphabet);
        //     Mata::Nfa::Nfa aut = util::conv_to_nfa(to_app(re), this->m_util_s, this->m, alphabet, !is_true);
        //     auto aut_lens = Mata::Strings::get_word_lengths(aut);

        //     expr_ref len_re(m_util_s.str.mk_length(re_constr), m);
        //     expr_ref len_formula = this->dec_proc.mk_len_aut(len_re, aut_lens);
        //     add_axiom({~mk_literal(re_atom), mk_literal(len_formula)});
        //     STRACE("str", tout << "re-axiom: " << mk_pp(len_formula, m) << "\n"; );
        // }
        
        expr_ref r{re, m};
        this->m_membership_todo.push_back(std::make_tuple(expr_ref(re_constr, m), r, is_true));
    }

    void theory_str_noodler::set_conflict(const literal_vector& lv) {
        context& ctx = get_context();
        const auto& js = ext_theory_conflict_justification{
                get_id(), ctx, lv.size(), lv.data(), 0, nullptr, 0, nullptr};
        ctx.set_conflict(ctx.mk_justification(js));
        STRACE("str", ctx.display_literals_verbose(tout << "[Conflict]\n", lv) << '\n';);
    }

    void theory_str_noodler::block_curr_assignment() {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);

        context& ctx = get_context();

        ast_manager& m = get_manager();
        expr *refinement = nullptr;
        STRACE("str", tout << "[Refinement]\nformulas:\n";);
        for (const auto& we : this->m_word_eq_todo_rel) {
            // we create the equation according to we
            //expr *const e = m.mk_not(m.mk_eq(we.first, we.second));
            expr *const e = m.mk_not(ctx.mk_eq_atom(we.first, we.second));
            refinement = refinement == nullptr ? e : m.mk_or(refinement, e);
            STRACE("str", tout << we.first << " = " << we.second << '\n';);
        }

        literal_vector ls;
        for (const auto& wi : this->m_word_diseq_todo_rel) {
//            expr *const e = mk_eq_atom(wi.first, wi.second);
            expr_ref e(ctx.mk_eq_atom(wi.first, wi.second), m);
            refinement = refinement == nullptr ? e : m.mk_or(refinement, e);
            STRACE("str", tout << wi.first << " != " << wi.second << " " << ctx.get_bool_var(e)<< '\n';);
        }

        for (const auto& in : this->m_membership_todo_rel) {
            app_ref in_app(m_util_s.re.mk_in_re(std::get<0>(in), std::get<1>(in)), m);
            if(std::get<2>(in)){
                in_app = m.mk_not(in_app);
                if(!ctx.e_internalized(in_app)) {
                    ctx.internalize(in_app, false);
                }
            }
            refinement = refinement == nullptr ? in_app : m.mk_or(refinement, in_app);
            //STRACE("str", tout << wi.first << " != " << wi.second << '\n';);
        }

        if (refinement != nullptr) {
            add_axiom(refinement);
        }
        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);

    }

    expr_ref theory_str_noodler::construct_refinement() {
        context& ctx = get_context();

        ast_manager& m = get_manager();
        expr *refinement = nullptr;
        STRACE("str", tout << "[Refinement]\nformulas:\n";);
        for (const auto& we : this->m_word_eq_todo_rel) {
            // we create the equation according to we
            //expr *const e = m.mk_not(m.mk_eq(we.first, we.second));
            expr *const e = ctx.mk_eq_atom(we.first, we.second);
            refinement = refinement == nullptr ? e : m.mk_and(refinement, e);
        }

        literal_vector ls;
        for (const auto& wi : this->m_word_diseq_todo_rel) {
//            expr *const e = mk_eq_atom(wi.first, wi.second);
            expr_ref e(m.mk_not(ctx.mk_eq_atom(wi.first, wi.second)), m);
            refinement = refinement == nullptr ? e : m.mk_and(refinement, e);
            //STRACE("str", tout << wi.first << " != " << wi.second << " " << ctx.get_bool_var(e)<< '\n';);
        }

        for (const auto& in : this->m_membership_todo_rel) {
            app_ref in_app(m_util_s.re.mk_in_re(std::get<0>(in), std::get<1>(in)), m);
            if(!std::get<2>(in)){
                in_app = m.mk_not(in_app);
                if(!ctx.e_internalized(in_app)) {
                    ctx.internalize(in_app, false);
                }
            }
            refinement = refinement == nullptr ? in_app : m.mk_and(refinement, in_app);
            //STRACE("str", tout << wi.first << " != " << wi.second << '\n';);
        }

        return expr_ref(refinement, m);
    }

    bool theory_str_noodler::block_curr_len(expr_ref len_formula) {
        STRACE("str", tout << __LINE__ << " enter " << __FUNCTION__ << std::endl;);

        context& ctx = get_context();

        ast_manager& m = get_manager();
        expr *refinement = nullptr;
        STRACE("str", tout << "[Refinement]\nformulas:\n";);
        for (const auto& we : this->m_word_eq_todo_rel) {
            // we create the equation according to we
            //expr *const e = m.mk_not(m.mk_eq(we.first, we.second));
            expr *const e = ctx.mk_eq_atom(we.first, we.second);
            refinement = refinement == nullptr ? e : m.mk_and(refinement, e);
        }

        literal_vector ls;
        for (const auto& wi : this->m_word_diseq_todo_rel) {
//            expr *const e = mk_eq_atom(wi.first, wi.second);
            expr_ref e(m.mk_not(ctx.mk_eq_atom(wi.first, wi.second)), m);
            refinement = refinement == nullptr ? e : m.mk_and(refinement, e);
            //STRACE("str", tout << wi.first << " != " << wi.second << " " << ctx.get_bool_var(e)<< '\n';);
        }

        for (const auto& in : this->m_membership_todo_rel) {
            app_ref in_app(m_util_s.re.mk_in_re(std::get<0>(in), std::get<1>(in)), m);
            if(!std::get<2>(in)){
                in_app = m.mk_not(in_app);
            }
            if(!ctx.e_internalized(in_app)) {
                ctx.internalize(in_app, false);
            }
            refinement = refinement == nullptr ? in_app : m.mk_and(refinement, in_app);
            //STRACE("str", tout << wi.first << " != " << wi.second << '\n';);
        }

        // if(axiomatized_instances.contains(refinement)) {
        //     return false;
        // }
        // axiomatized_instances.push_back(refinement);
        if(m_params.m_loop_protect) {
            this->axiomatized_instances.push_back({expr_ref(refinement, this->m), len_formula});
        }
        if (refinement != nullptr) {
            add_axiom(m.mk_or(m.mk_not(refinement), len_formula));
        }
        STRACE("str", tout << __LINE__ << " leave " << __FUNCTION__ << std::endl;);
        return true;
    }

    void theory_str_noodler::dump_assignments() const {
        STRACE("str", \
                ast_manager& m = get_manager();
                context& ctx = get_context();
                std::cout << "dump all assignments:\n";
                expr_ref_vector assignments{m};


                ctx.get_assignments(assignments);
                for (expr *const e : assignments) {
                   // ctx.mark_as_relevant(e);
                    std::cout <<"**"<< mk_pp(e, m) << (ctx.is_relevant(e) ? "\n" : " (not relevant)\n");
                }
        );
    }

    expr_ref theory_str_noodler::mk_str_var(const std::string& name) {
        // TODO remove this function probably and just use the one from util
        return util::mk_str_var_fresh(name, m, m_util_s);
    }

    expr_ref theory_str_noodler::mk_int_var(const std::string& name) {
        // TODO remove this function probably and just use the one from util
        return util::mk_int_var_fresh(name, m, m_util_a);
    }

    Predicate theory_str_noodler::conv_eq_pred(app* const ex) {
        const app* eq = ex;
        PredicateType ptype = PredicateType::Equation;
        if(m.is_not(ex)) {
            SASSERT(is_app(ex->get_arg(0)));
            SASSERT(ex->get_num_args() == 1);
            eq = to_app(ex->get_arg(0));
            ptype = PredicateType::Inequation;
        }
        SASSERT(eq->get_num_args() == 2);
        SASSERT(eq->get_arg(0));
        SASSERT(eq->get_arg(1));

        obj_hashtable<expr> vars;
        util::get_str_variables(ex, this->m_util_s, this->m, vars);
        for(expr * const v : vars) {

            BasicTerm vterm(BasicTermType::Variable, to_app(v)->get_name().str());
            this->var_name.insert({vterm, expr_ref(v, this->m)});
        }

        std::vector<BasicTerm> left, right;
        util::collect_terms(to_app(eq->get_arg(0)), m, this->m_util_s, this->predicate_replace, this->var_name, left);
        util::collect_terms(to_app(eq->get_arg(1)), m, this->m_util_s, this->predicate_replace, this->var_name, right);

        return Predicate(ptype, std::vector<std::vector<BasicTerm>>{left, right});
    }

    void theory_str_noodler::conj_instance(const obj_hashtable<app>& conj, Formula &res) {
        for(app *const pred : conj) {
            //print_ast(pred);
            Predicate inst = this->conv_eq_pred(pred);
            STRACE("str", tout  << "instance conversion " << inst.to_string() << std::endl;);
            res.add_predicate(inst);
        }
    }

    std::unordered_set<BasicTerm> theory_str_noodler::get_init_length_vars(AutAssignment& ass) {
        std::unordered_set<BasicTerm> init_lengths{};
        for (const auto& len : len_vars) {
            BasicTerm v = util::get_variable_basic_term(len);
            if(ass.find(v) != ass.end())
                init_lengths.emplace(v);
        }
        return init_lengths;
    }

    lbool theory_str_noodler::check_len_sat(expr_ref len_formula, model_ref &mod) {
        int_expr_solver m_int_solver(get_manager(), get_context().get_fparams());
        // do we solve only regular constraints? If yes, skip other temporary length constraints (they are not necessary)
        bool include_ass = true;
        if(this->m_word_diseq_todo_rel.size() == 0 && this->m_word_eq_todo_rel.size() == 0) {
            include_ass = false;
        }
        m_int_solver.initialize(get_context(), include_ass);
        auto ret = m_int_solver.check_sat(len_formula);
        return ret;
    }
}
