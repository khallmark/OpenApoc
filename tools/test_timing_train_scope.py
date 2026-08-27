#!/usr/bin/env python3
"""Structural locks for the render/simulation train contract."""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCOPE_PATH = ROOT / "docs" / "timing" / "train-scope-v1.json"
HARNESS_WORKFLOW_PATH = ROOT / ".github" / "workflows" / "harness.yml"
COMMON_ATTEMPT_ID = "common-intent-attempt-fence-v1"


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
	value: dict[str, Any] = {}
	for key, item in pairs:
		if key in value:
			raise ValueError(f"duplicate JSON key: {key}")
		value[key] = item
	return value


def load_scope() -> dict[str, Any]:
	return json.loads(SCOPE_PATH.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)


def all_strings(value: Any):
	if isinstance(value, str):
		yield value
	elif isinstance(value, dict):
		for item in value.values():
			yield from all_strings(item)
	elif isinstance(value, list):
		for item in value:
			yield from all_strings(item)


class TrainScopeTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		cls.scope = load_scope()
		cls.contracts = cls.scope["contracts"]
		cls.slices = {item["id"]: item for item in cls.scope["slices"]}

	def ancestors(self, slice_id: str) -> set[str]:
		seen: set[str] = set()
		pending = list(self.slices[slice_id]["depends_on"])
		while pending:
			dependency = pending.pop()
			if dependency in seen:
				continue
			seen.add(dependency)
			pending.extend(self.slices[dependency]["depends_on"])
		return seen

	def test_contract_registry_is_closed_and_unique(self) -> None:
		self.assertEqual(
			set(self.contracts),
			{
				"oracle_calibration",
				"activation_oracle_topology",
				"replay_schema_upgrade",
				"batch_attempt_protocol",
				"nightly_schedule",
				"ordinary_admission",
				"successor_release",
				"compatibility_delta",
			},
		)
		contract_ids = [contract["id"] for contract in self.contracts.values()]
		self.assertEqual(len(contract_ids), len(set(contract_ids)))
		self.assertNotIn(None, contract_ids)

	def test_slice_graph_is_unique_known_and_acyclic(self) -> None:
		self.assertEqual(len(self.scope["slices"]), 37)
		self.assertEqual(len(self.slices), 37)
		for slice_id, item in self.slices.items():
			self.assertEqual(len(item["depends_on"]), len(set(item["depends_on"])), slice_id)
			self.assertTrue(set(item["depends_on"]).issubset(self.slices), slice_id)
			self.assertNotIn(slice_id, self.ancestors(slice_id), slice_id)

	def test_completion_partition_and_successor_release_are_exact(self) -> None:
		partition = self.scope["completion"]["predicate"]["slice_proof_partition"]
		preledger = set(partition["preledger_ancestry_proof_slice_ids"])
		postledger = set(partition["land_exact_terminal_slice_ids"])
		self.assertEqual(preledger & postledger, set())
		self.assertEqual(preledger | postledger, set(self.slices))
		self.assertEqual(len(preledger), 11)
		self.assertEqual(len(postledger), 26)
		self.assertEqual(preledger, self.ancestors("R1b"))
		self.assertEqual(
			postledger,
			set(self.contracts["successor_release"]["applies_after_slice_ids"]),
		)

	def test_planning_scope_trust_root_is_base_owned_and_head_bound(self) -> None:
		delivery = self.scope["planning_delivery"]
		self.assertEqual(self.slices["P0"]["depends_on"], ["Z"])
		self.assertEqual(self.slices["P"]["depends_on"], ["P0"])
		trust = delivery["trust_root"]
		self.assertEqual(trust["owner"], "landed_default_branch_P0_only")
		self.assertEqual(
			trust["owned_artifacts"],
			[
				".github/workflows/cmake.yml",
				".github/workflows/harness.yml",
				".github/workflows/lint.yml",
				".github/workflows/planning-scope.yml",
				"docs/timing/pr-p-policy-v1.json",
				"tools/validate_pr_p_scope.py",
			],
		)
		self.assertFalse(trust["P_may_modify_any_owned_artifact"])
		self.assertIn("cannot_validate_itself", trust["bootstrap_genesis"])
		workflow = delivery["trusted_workflow"]
		self.assertEqual(workflow["event"], "pull_request_target")
		self.assertTrue(workflow["always_run"])
		self.assertEqual(workflow["permissions"], ["contents:read"])
		self.assertEqual(workflow["untrusted_checkout_import_execution_shell_action_build_test_submodule_lfs_filter_or_hook"], "forbidden")
		self.assertIn("python_-I_-S_-B", workflow["tagged_validator_execution"])
		ordinary = workflow["ordinary_workflow_hardening"]
		self.assertEqual(
			ordinary["exact_paths"],
			[
				".github/workflows/cmake.yml",
				".github/workflows/harness.yml",
				".github/workflows/lint.yml",
			],
		)
		self.assertEqual(ordinary["permissions"], ["contents:read"])
		self.assertEqual(ordinary["checkout_action"], "actions_checkout_v7_0_1_full_commit_SHA")
		self.assertFalse(ordinary["persist_credentials"])
		self.assertTrue(ordinary["workflow_bytes_frozen_by_P0"])
		self.assertFalse(workflow["ordinary_harness"]["persist_credentials"])
		self.assertEqual(workflow["ordinary_harness"]["permissions"], ["contents:read"])
		binding = delivery["immutable_event_and_git_binding"]
		self.assertEqual(binding["P_identity_from_P0_policy"]["base_ref"], "develop")
		self.assertEqual(binding["P_identity_from_P0_policy"]["head_ref"], "khallmark/render-simulation-design")
		self.assertEqual(binding["head_manifest_or_changed_path_detection_selects_P"], "forbidden")
		self.assertIn("fresh_bare_repo", binding["isolated_repository"])
		self.assertIn("event_head_sha", binding["fetch"])
		self.assertIn("exactly_one_nonmerge_commit", binding["history_rule"])
		self.assertIn("parent_is_event_base", binding["history_rule"])
		self.assertIn("GITHUB_WORKFLOW_SHA", binding["required_values"])
		self.assertTrue(any("both_tag_refs_are_direct_annotated_tag_objects" in item for item in binding["required_git_predicates"]))
		self.assertTrue(any("complete_event_base_and_head_workflow_tree" in item for item in binding["required_git_predicates"]))
		policy = delivery["P0_canonical_policy"]
		self.assertEqual(policy["exact_changed_path_count"], 22)
		self.assertIn("exact_22_path", policy["artifact_authority"])
		self.assertIn("sha256_by_path", policy["artifact_authority"])
		self.assertIn("never_authority", policy["head_manifest_role"])
		self.assertIn("all_22_P_artifacts_absent", policy["non_P_closed_state_machine"]["pre_P_base"])
		self.assertIn(".github/workflows", policy["protected_workflow_tree"])
		protection = delivery["required_check_and_protection_activation"]
		self.assertIn("check_run_head_sha_equals_event_head_sha", protection["sacrificial_post_P0_run"])
		self.assertTrue(protection["protection_read_back_after_configuration"])
		self.assertEqual(protection["skipped_neutral_stale_prior_head_wrong_app_or_context_collision"], "reject")
		trust = delivery["trust_root"]
		self.assertEqual(trust["trust_tag_ref"], "refs/tags/planning-scope-trust-v1")
		self.assertEqual(trust["receipt_tag_ref"], "refs/tags/planning-scope-trust-v1-receipt")
		self.assertIn("prove_both_fixed_tag_refs_absent", trust["bootstrap_sequence"])
		self.assertTrue(any("atomically_create_only_push" in item for item in trust["bootstrap_sequence"]))
		self.assertEqual(trust["bootstrap_crash_mismatch_or_preexisting_ref"], "hard_red_no_delete_recreate_retarget_or_bypass_recovery")

	def test_robot_control_and_parity_dependencies_are_closed(self) -> None:
		self.assertEqual(set(self.slices["R1c"]["depends_on"]), {"R1a", "R1v"})
		self.assertIn("R1c", self.slices["R1p"]["depends_on"])
		self.assertIn("A", self.slices["R1p"]["depends_on"])
		self.assertEqual(self.slices["R1o"]["depends_on"], ["R1p"])
		self.assertIn("A", self.ancestors("R1o"))
		self.assertIn("A", self.ancestors("R1b"))
		for slice_id in self.slices:
			if slice_id.startswith(("P-", "T-")):
				self.assertIn("R1b", self.ancestors(slice_id), slice_id)

	def test_common_attempt_protocol_covers_every_executable_kind(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		rows = protocol["intent_kind_table"]
		self.assertEqual(protocol["id"], COMMON_ATTEMPT_ID)
		self.assertEqual(len(rows), 12)
		self.assertEqual(len({row["kind"] for row in rows}), 12)
		self.assertTrue(protocol["closed_intent_kind_table"])
		self.assertEqual(protocol["schema_consistency_predicate"]["exact_executable_intent_kind_count"], 12)
		for name, contract in self.contracts.items():
			if name not in {"activation_oracle_topology", "batch_attempt_protocol"}:
				self.assertEqual(contract["attempt_protocol_id"], COMMON_ATTEMPT_ID, name)
		infra_terminals = {value for value in all_strings(self.scope) if value.endswith("infrastructure-failed")}
		self.assertEqual(infra_terminals, {"attempt-infrastructure-failed"})
		self.assertNotIn("attempt_claim_jcs_sha256", protocol["attempt_started_required_bindings"])
		for binding in ("runtime_launch_template_jcs_sha256", "process_launch_plan_authority_jcs_sha256"):
			self.assertIn(binding, protocol["attempt_started_required_bindings"])
		self.assertEqual(protocol["attempt_execution_began_record_type"], "attempt-execution-began")
		for binding in (
			"intent_jcs_sha256",
			"attempt_started_jcs_sha256",
			"attempt_claim_jcs_sha256",
			"attempt_fence_jcs_sha256",
			"manifest_jcs_sha256",
			"tested_sha",
			"tested_tree",
			"runtime_launch_template_jcs_sha256",
			"process_launch_plan_authority_jcs_sha256",
			"execution_linearization_monotonic_ns",
		):
			self.assertIn(binding, protocol["attempt_execution_began_required_bindings"])
		self.assertTrue(protocol["every_worker_spawn_claim_receipt_progress_and_terminal_requires_attempt_execution_began_jcs_sha256"])
		self.assertTrue(protocol["attempt_infrastructure_failed_forbids_attempt_execution_began_or_descendant_artifact"])
		self.assertIn("before_attempt-execution-began", protocol["retryable_infrastructure_window"])
		self.assertIn("no_worker_or_subject_process_spawn_receipt", protocol["infrastructure_failure_proof"])
		self.assertIn("logical_failure", protocol["execution_began_or_ambiguous_spawn_effect"])
		self.assertIn("logical_failure", protocol["crash_after_execution_began_before_actual_spawn"])
		self.assertTrue(protocol["schema_consistency_predicate"]["infrastructure_terminal_is_legal_only_before_attempt_execution_began"])
		self.assertTrue(protocol["schema_consistency_predicate"]["execution_began_interruption_is_logical_failure"])

	def test_runtime_launch_intent_and_realized_process_truth_are_separate(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		family = protocol["runtime_execution_closure"]
		self.assertEqual(family["schema_family"], "openapoc.runtime_execution_closure.v1")
		self.assertEqual(family["authority_slice_id"], "R1p")
		template = family["runtime_launch_template"]
		self.assertEqual(template["schema"], "openapoc.runtime_launch_template.v1")
		self.assertEqual(template["digest_field"], "runtime_launch_template_jcs_sha256")
		self.assertEqual(template["process_nonce_port_output_root_HOME_TMPDIR_save_directory_PID_start_identity_actual_argv_actual_environment_and_runtime_image_observation"], "forbidden")
		self.assertIn("pid", template["forbidden_fields"])
		self.assertIn("observed_runtime_images", template["forbidden_fields"])
		self.assertTrue(template["runtime_invariant_projection_schema_is_closed_and_candidate_cannot_widen_exclusions"])
		planned = family["planned_process_launch_set"]
		self.assertEqual(planned["schema"], "openapoc.planned_process_launch_set.v1")
		self.assertEqual(planned["planning_mode"], "finite_exact")
		self.assertIn("same_runtime_launch_template", planned["oracle_36_and_54_rule"])
		self.assertIn("distinct_phase_launch_sets", planned["oracle_36_and_54_rule"])
		self.assertTrue(planned["campaign_count_and_total_process_count_are_distinct"])
		self.assertNotIn("argv_jcs_sha256", planned["entry_required_fields"])
		self.assertNotIn("input_closure_jcs_sha256", planned["entry_required_fields"])
		variants = planned["launch_binding_variants"]
		self.assertEqual(set(variants), {"campaign_initial", "continuation_from_receipt"})
		self.assertIn("input_closure_jcs_sha256", variants["campaign_initial"]["required_fields"])
		continuation = variants["continuation_from_receipt"]
		for field in (
			"producer_process_launch_id",
			"producer_output_selector_jcs_sha256",
			"allowed_relaunch_delta_policy_jcs_sha256",
		):
			self.assertIn(field, continuation["required_fields"])
		self.assertIn("produced_save_jcs_sha256", continuation["preexecution_forbidden_future_fields"])
		self.assertIn("orderly_reap", continuation["materialization_rule"])
		self.assertTrue(planned["continuation_row_never_claims_future_save_or_argv_bytes"])
		authority = family["process_launch_plan_authority"]
		self.assertEqual(set(authority["closed_variants"]), {"finite_exact", "deterministic_gap_free_prefix"})
		self.assertTrue(authority["attempt_started_and_attempt_execution_began_bind_only_this_immutable_discriminated_authority"])
		prefix = family["planned_process_launch_prefix_policy"]
		self.assertEqual(prefix["digest_field"], "planned_process_launch_prefix_policy_jcs_sha256")
		self.assertEqual(prefix["finalized_prefix_digest_field"], "finalized_process_launch_prefix_jcs_sha256")
		self.assertEqual(
			set(prefix["finalized_prefix_terminal_required_fields"]),
			{
				"finalized_campaign_root_prefix_jcs_sha256",
				"finalized_campaign_root_prefix_count",
				"finalized_campaign_root_prefix_head_jcs_sha256",
				"finalized_process_launch_prefix_jcs_sha256",
				"finalized_process_launch_prefix_count",
				"finalized_process_launch_prefix_head_jcs_sha256",
			},
		)
		self.assertNotIn("launch_cutoff_monotonic_ns", prefix["immutable_before_attempt_started_fields"])
		self.assertIn("duration_ns", prefix["immutable_before_attempt_started_fields"])
		self.assertEqual(set(prefix["closed_prefix_chains"]), {"campaign_root", "process_launch"})
		self.assertIn("before_cutoff", prefix["closed_prefix_chains"]["campaign_root"]["append_rule"])
		self.assertIn("root_or_continuation", prefix["closed_prefix_chains"]["process_launch"]["append_rule"])
		self.assertIn("never_allocates_a_campaign_ordinal", prefix["closed_prefix_chains"]["process_launch"]["continuation_binding"])
		self.assertEqual(prefix["every_prefix_entry_required_binding"], "soak_execution_window_jcs_sha256")
		window = family["soak_execution_window"]
		self.assertEqual(window["schema"], "openapoc.soak_execution_window.v1")
		self.assertEqual(window["digest_field"], "soak_execution_window_jcs_sha256")
		self.assertIn("atomically", window["publication"])
		self.assertIn("origin_plus_duration", window["cutoff_rule"])
		self.assertIn("fresh_window", window["pre_execution_retry"])
		self.assertIn("never_retryable", window["post_publication_interruption"])
		root_start = family["soak_campaign_root_start_receipt"]
		self.assertEqual(root_start["schema"], "openapoc.soak_campaign_root_start_receipt.v1")
		for binding in (
			"soak_execution_window_jcs_sha256",
			"monotonic_clock_domain_jcs_sha256",
			"materialized_process_launch_jcs_sha256",
			"pid_start_identity_jcs_sha256",
			"process_instance_nonce",
			"root_start_monotonic_ns",
		):
			self.assertIn(binding, root_start["required_bindings"])
		self.assertIn("coordinator_samples", root_start["timestamp_authority"])
		self.assertIn("strictly_less_than_launch_cutoff", root_start["cutoff_predicate"])
		self.assertIn("checked_uint64_add_root_start", root_start["drain_deadline_rule"])
		self.assertIn("root_start_receipts", root_start["success_bijection"])
		cardinalities = family["campaign_cardinality_bindings"]
		for name, expected in (
			("pre_oracle_projection_stability", 36),
			("oracle_calibration", 54),
			("ordinary_candidate_admission", 3),
			("integrated_landed_head_repeat", 20),
			("independent_nightly_batch", 20),
			("activation", 100),
		):
			self.assertEqual(cardinalities[name]["campaign_count"], expected, name)
			self.assertEqual(cardinalities[name]["initial_process_launch_row_count"], expected, name)
		self.assertIn("additional", cardinalities["save_reload_continuation_process_rows"])
		self.assertIn("campaign_ordinal_prefix", cardinalities["soak_campaign_root_rows"])
		self.assertIn("process_launch_ordinal_prefix", cardinalities["soak_process_rows"])
		materialized = family["materialized_process_launch"]
		self.assertEqual(materialized["schema"], "openapoc.materialized_process_launch.v1")
		self.assertIn("attempt-execution-began", materialized["publication"])
		realized = family["realized_runtime_closure_receipt"]
		self.assertEqual(realized["schema"], "openapoc.realized_runtime_closure_receipt.v1")
		self.assertTrue(realized["full_realized_closure_expected_to_differ_per_process"])
		self.assertEqual(realized["required_equality_surface"], "closed_runtime_invariant_projection_only")
		self.assertIn("partial_realized", realized["logical_failure_evidence"])
		self.assertIn("soak_campaign_root_start_receipt_jcs_sha256", realized["soak_receipt_additionally_requires"])
		self.assertIn("continuation", realized["soak_root_start_binding_rule"])
		terminal = family["execution_terminal_binding"]
		self.assertEqual(terminal["success_after_attempt_execution_began_requires"], "realized_runtime_closure_receipt_set_jcs_sha256")
		self.assertEqual(
			set(terminal["logical_failure_after_attempt_execution_began_requires"]),
			{"partial_realized_runtime_closure_evidence_set_jcs_sha256", "unfulfilled_process_launch_id_set_jcs_sha256"},
		)
		self.assertIn("realized_runtime_closure_receipt_set_jcs_sha256", terminal["attempt_infrastructure_failed_forbids"])
		self.assertEqual(terminal["finite_exact_success_additionally_requires"], "planned_process_launch_set_jcs_sha256")
		self.assertEqual(
			set(terminal["deterministic_gap_free_prefix_terminal_additionally_requires"]),
			{
					"planned_process_launch_prefix_policy_jcs_sha256",
					"soak_execution_window_jcs_sha256",
					"soak_campaign_root_start_receipt_set_jcs_sha256",
				"finalized_campaign_root_prefix_jcs_sha256",
				"finalized_campaign_root_prefix_count",
				"finalized_campaign_root_prefix_head_jcs_sha256",
				"finalized_process_launch_prefix_jcs_sha256",
				"finalized_process_launch_prefix_count",
				"finalized_process_launch_prefix_head_jcs_sha256",
			},
		)
		strings = set(all_strings(self.scope))
		self.assertNotIn("runtime_closure_jcs_sha256", strings)
		self.assertNotIn("runtime_closure_digest", strings)
		for contract_name in ("successor_release", "nightly_schedule"):
			required = self.contracts[contract_name]["required_bindings"] if contract_name == "successor_release" else self.contracts[contract_name]["manifest"]["required_bindings"]
			for binding in ("runtime_launch_template_jcs_sha256", "process_launch_plan_authority_jcs_sha256", "planned_process_launch_set_jcs_sha256", "realized_runtime_closure_receipt_set_jcs_sha256"):
				self.assertIn(binding, required, contract_name)
		for required in (
			self.contracts["oracle_calibration"]["projection_stability_probe"]["required_bindings"],
			self.contracts["oracle_calibration"]["victory_oracle_required_bindings"],
		):
			for binding in (
				"runtime_launch_template_jcs_sha256",
				"process_launch_plan_authority_jcs_sha256",
				"planned_process_launch_set_jcs_sha256",
				"realized_runtime_closure_receipt_set_jcs_sha256",
			):
				self.assertIn(binding, required)

	def test_attempt_terminals_are_disjoint_from_lifecycle_transitions(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		rows = {row["kind"]: row for row in protocol["intent_kind_table"]}
		self.assertEqual(rows["ordinary-admission"]["logical_failure_record_types"], ["admission-failed"])
		self.assertEqual(rows["replay-schema-upgrade"]["success_record_types"], ["replay-schema-activated"])
		self.assertEqual(rows["replay-schema-upgrade"]["logical_failure_record_types"], ["replay-schema-failed"])
		self.assertEqual(
			set(rows["compatibility-delta"]["logical_failure_record_types"]),
			{"compatibility-delta-failed", "parent-reference-mismatch"},
		)
		attempt_terminals = {
			record_type
			for row in rows.values()
			for key in ("success_record_types", "logical_failure_record_types")
			for record_type in row[key]
		}
		lifecycle = protocol["post_success_lifecycle_transitions"]
		self.assertEqual(
			set(lifecycle["record_types"]),
			{"revoked", "replay-schema-invalidated", "compatibility-delta-invalidated"},
		)
		self.assertTrue(attempt_terminals.isdisjoint(lifecycle["record_types"]))
		self.assertTrue(lifecycle["outside_attempt_terminal_table"])
		self.assertTrue(lifecycle["cannot_reterminalize_or_reuse_the_successful_attempt"])

	def test_compound_ledger_transitions_are_one_atomic_envelope(self) -> None:
		envelope = self.contracts["batch_attempt_protocol"]["ledger_transaction_envelope"]
		self.assertEqual(envelope["schema"], "openapoc.admission_ledger_transaction.v1")
		self.assertIn("atomic_no_replace_rename", envelope["physical_append_unit"])
		self.assertIn("never_a_prefix", envelope["visibility"])
		self.assertTrue(envelope["attempt_terminal_inside_compound_envelope_does_not_release_mutex_early"])
		self.assertIn("operation_terminal_envelope", envelope["operation_mutex_release"])
		self.assertIn("never_release_an_open_operation", envelope["operation_mutex_release"])
		compound = envelope["compound_transition_sets"]
		self.assertTrue(envelope["compound_transition_selection_is_context_total_and_mutually_exclusive"])
		self.assertEqual(
			compound["candidate-epoch-ledger-open-v1"]["ordered_logical_record_types"],
			["epoch-init", "candidate"],
		)
		self.assertEqual(
			compound["baseline-only-noop-land-v1"]["ordered_logical_record_types"],
			["land-exact-intent", "land-exact-completed"],
		)
		self.assertEqual(
			compound["land-abort-reconcile-open-v1"]["ordered_logical_record_types"],
			["land-exact-aborted", "land-exact-reconcile-intent"],
		)
		self.assertEqual(
			compound["land-reconcile-failure-revocation-recovery-authority-v1"]["ordered_logical_record_types"],
			["land-exact-reconcile-failed", "revoked", "land-recovery-authorized"],
		)
		self.assertEqual(
			compound["land-recovery-reauthorization-v1"]["ordered_logical_record_types"],
			["land-recovery-reauthorized"],
		)
		self.assertEqual(
			compound["G-admission-pending-v1"]["ordered_logical_record_types"],
			["admitted", "g-gates-pending"],
		)
		red = compound["admitted-subject-logical-red-revocation-v1"]
		self.assertEqual(red["ordered_logical_record_types"], ["selected_failure_record_type", "revoked"])
		for failure in (
			"integrated-repeat-failed",
			"nightly-failed",
			"desktop-presentation-failed",
			"desktop-performance-failed",
			"activation-failed",
			"soak-failed",
		):
			self.assertIn(failure, red["failure_record_type_domain"])

		transaction = {"logical_records": ["admitted", "g-gates-pending"]}
		visible_before_rename: list[str] = []
		visible_after_rename = list(transaction["logical_records"])
		self.assertEqual(visible_before_rename, [])
		self.assertEqual(visible_after_rename, ["admitted", "g-gates-pending"])
		self.assertNotEqual(visible_after_rename, ["admitted"])

		closed = set(envelope["closed_logical_record_types"])
		self.assertIn("land-recovery-authorized", closed)
		self.assertIn("land-recovery-reauthorized", closed)
		self.assertIn("train-abandon-requested", closed)
		dynamic_members = {
			"selected_failure_record_type",
			"selected_invalidation_record_type",
			"selected_intent_kind_logical_failure_record_type",
		}
		for vector in compound.values():
			for record_type in vector["ordered_logical_record_types"]:
				self.assertTrue(record_type in closed or record_type in dynamic_members, record_type)

	def test_train_abandonment_atomically_closes_the_only_open_intent(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		abandonment = protocol["train_abandonment"]
		self.assertEqual(abandonment["authority_role"], "train_owner")
		self.assertIn("same_mutex_owned_operation", abandonment["when_one_operation_is_open"])
		self.assertIn("logical_failure", abandonment["live_execution_abandonment"])
		self.assertEqual(abandonment["pre_execution_open_intent_terminal"], "intent-abandoned")
		self.assertIn("attempt-infrastructure-failed", abandonment["pre_execution_live_attempt_rule"])
		self.assertIn("defined_crash_recovery", abandonment["non_intent_open_operation_rule"])
		self.assertTrue(abandonment["all_processes_and_attempts_terminal_before_transaction"])
		self.assertEqual(abandonment["intent-abandoned_outside_exact_train_abandonment_envelope"], "invalid_ledger")
		self.assertEqual(abandonment["retry_resume_or_new_success_after_train-abandoned"], "forbidden")
		compound = protocol["ledger_transaction_envelope"]["compound_transition_sets"]
		self.assertEqual(
			compound["pre-execution-intent-abandon-train-v1"]["ordered_logical_record_types"],
			["intent-abandoned", "train-abandoned"],
		)
		self.assertEqual(
			compound["unadmitted-logical-red-train-abandon-v1"]["ordered_logical_record_types"],
			["selected_intent_kind_logical_failure_record_type", "train-abandoned"],
		)
		self.assertEqual(
			compound["admitted-logical-red-revocation-train-abandon-v1"]["ordered_logical_record_types"],
			["selected_intent_kind_logical_failure_record_type", "revoked", "train-abandoned"],
		)
		expected_g_vectors = {
			"G-pending-idle-abandon-v1": ["revoked", "train-abandoned"],
			"G-pending-pre-execution-gate-abandon-v1": ["intent-abandoned", "revoked", "train-abandoned"],
			"G-pending-live-pre-execution-gate-abandon-v1": ["attempt-infrastructure-failed", "intent-abandoned", "revoked", "train-abandoned"],
			"G-pending-land-success-abandon-v1": ["land-exact-completed", "train-abandoned"],
			"G-pending-land-abort-reconcile-abandon-request-v1": ["land-exact-aborted", "land-exact-reconcile-intent", "train-abandon-requested"],
			"G-pending-reconcile-applied-abandon-v1": ["land-exact-reconciled", "train-abandoned"],
			"G-pending-reconcile-not-applied-abandon-v1": ["land-exact-reconcile-not-applied", "revoked", "train-abandoned"],
			"G-pending-reconcile-failed-abandon-v1": ["land-exact-reconcile-failed", "revoked", "train-abandoned"],
		}
		for vector_id, ordered_types in expected_g_vectors.items():
			self.assertEqual(compound[vector_id]["ordered_logical_record_types"], ordered_types, vector_id)

		dispatch = abandonment["G_pending_dispatch"]
		self.assertTrue(dispatch["selection_is_total_and_mutually_exclusive"])
		self.assertEqual(
			dispatch["selection_order"],
			["open_reconciliation", "open_land", "open_gate_partitioned_by_attempt-execution-began", "idle_G_pending"],
		)
		self.assertEqual(
			set(dispatch["idle_positions"]),
			{"before_desktop_presentation", "after_desktop_presentation", "after_desktop_performance", "after_activation", "after_soak_before_land", "after_land_exact_reconcile_not_applied"},
		)
		self.assertTrue(dispatch["every_abandonment_path_closes_g_gates_pending_by_G_land_or_revocation"])
		self.assertEqual(dispatch["normal_reconciliation_terminal_after_train-abandon-requested"], "invalid_ledger")

	def test_parent_reference_mismatch_closes_old_compatibility_intent(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		self.assertTrue(
			protocol["parent_reference_mismatch_terminalizes_compatibility_attempt_before_parent_audit_or_fresh_parent_first_intent"]
		)
		audit = self.contracts["compatibility_delta"]["parent_reference_audit"]
		self.assertIn("terminalize_and_burn_current_compatibility_delta_intent_attempt_and_fence", audit["immediate_effect"])
		self.assertIn("new_parent_first_compatibility_delta_intent", audit["transitions"]["not_reproduced"])
		self.assertIn("terminalize_fresh_compatibility_intent_and_attempt", audit["transitions"]["fresh_parent_confirmation_mismatches_again_before_limit"])
		fresh = audit["fresh_compatibility_attempt_after_not_reproduced"]
		self.assertIn("before_launching_delta_source_half", fresh["execution_order"])
		self.assertIn("keep_parent_clear", fresh["delta_source_logical_failure_after_clear"])
		for binding in (
			"fresh_parent_first_compatibility_intent_jcs_sha256",
			"fresh_parent_first_attempt_started_jcs_sha256",
			"fresh_parent_first_attempt_claim_jcs_sha256",
			"fresh_parent_first_attempt_fence_jcs_sha256",
			"fresh_parent_replay_complete_terminal_receipt_set_jcs_sha256",
			"fresh_parent_campaign_complete_terminal_receipt_set_jcs_sha256",
			"fresh_parent_replay_consensus_set_jcs_sha256",
			"fresh_parent_campaign_consensus_set_jcs_sha256",
		):
			self.assertIn(binding, audit["cleared_record_required_bindings"])
		self.assertFalse(any("delta_source" in binding for binding in audit["cleared_record_required_bindings"]))

	def test_parent_resolution_variants_and_construction_are_closed(self) -> None:
		ordinary = self.contracts["ordinary_admission"]
		resolution = ordinary["candidate_parent_resolution"]
		variants = resolution["transition_variants"]
		self.assertEqual(set(variants), set(ordinary["applies_to"]))
		for variant in variants.values():
			self.assertIn("exact_current_remote_develop", variant["transport_parent"])
		merge = resolution["candidate_construction_modes"]["reviewed-source-merge"]
		self.assertEqual(merge["exact_parent_count"], 2)
		self.assertEqual(merge["ordered_parents"], ["transport_parent", "exact_reviewed_source_head"])
		self.assertTrue(merge["transport_parent_must_be_ancestor_of_exact_reviewed_source_head"])
		self.assertTrue(merge["reviewed_source_pr_diff_base_must_equal_transport_parent"])
		self.assertTrue(merge["every_source_tree_delta_from_transport_must_be_in_exact_reviewed_pr_patch"])
		self.assertIn("reject", merge["stale_sibling_source_head_not_descending_from_current_transport"])
		self.assertTrue(merge["candidate_tree_must_equal_reviewed_source_tree"])
		self.assertEqual(merge["land_compare_and_swap_expected_old_sha_tree"], "transport_parent")
		noop = resolution["candidate_construction_modes"]["baseline-only-noop"]
		self.assertEqual(set(noop["allowed_transition_variants"]), {"recovery-root", "ordinary-recovery"})
		self.assertIn("baseline_invalidated", noop["authorization_condition"])
		self.assertIn("land_recovery_authority_jcs_sha256", noop["authorization_condition"])
		self.assertIn("either_closed_variant", noop["authorization_condition"])
		self.assertIn("receipt-authority-only", noop["authorization_condition"])
		self.assertIn("zero_robot_red", noop["authorization_condition"])
		self.assertEqual(
			set(noop["baseline_only_noop_authority"]["closed_variants"]),
			{"baseline-invalidation", "land-recovery"},
		)
		self.assertEqual(noop["any_robot_red_member"], "forbid_noop_and_require_reviewed_code_corrected_source_tree")
		self.assertTrue(noop["candidate_sha_tree_must_exactly_equal_transport_parent"])
		self.assertEqual(noop["source_or_candidate_commit_creation"], "forbidden")
		self.assertEqual(noop["develop_ref_mutation"], "forbidden")
		self.assertEqual(noop["immutable_epoch_ref_create_only_publication_at_transport_sha_tree"], "required")
		self.assertEqual(noop["epoch_ref_update_delete_force_or_other_remote_mutation"], "forbidden")
		for variant_id in ("genesis", "recovery-root"):
			self.assertEqual(variants[variant_id]["logical_parent_fields"], "forbidden")
			self.assertIn("baseline", variants[variant_id]["acceptance_oracle_authority"])
		for variant_id in ("ordinary-recovery", "integration-merge"):
			self.assertIn("logical_parent", variants[variant_id])
		self.assertEqual(resolution["resolution_digest_field"], "candidate_parent_resolution_jcs_sha256")
		self.assertTrue(resolution["resolution_digest_binds_every_candidate_intent_attempt_claim_receipt_admission_failure_revocation_and_land_surface"])
		recovery_bindings = resolution["recovery_candidate_authority_bindings"]
		self.assertEqual(set(recovery_bindings["applies_to_construction_modes"]), {"reviewed-source-merge", "baseline-only-noop"})
		self.assertEqual(
			set(recovery_bindings["required_fields"]),
			{
				"land_recovery_authority_jcs_sha256",
				"land_recovery_authority_variant",
				"lineage_root_land_recovery_authorized_jcs_sha256",
				"authority_generation",
				"recovery_mode",
				"derived_remediates_set_jcs_sha256",
			},
		)
		self.assertEqual(recovery_bindings["when_condition_is_false"], "all_fields_forbidden")
		publication = ordinary["candidate_epoch_publication"]
		self.assertEqual(publication["operation_kind"], "candidate-epoch-publication")
		self.assertTrue(publication["may_not_overlap_any_other_closed_operation_kind"])
		self.assertEqual(publication["ledger_open_ordered_logical_record_types"], ["epoch-init", "candidate"])
		self.assertEqual(publication["success_terminal_record_type"], "candidate-ref-published")
		self.assertEqual(publication["failure_terminal_record_type"], "candidate-publication-failed")
		self.assertEqual(publication["success_terminal_digest_field"], "candidate_ref_published_jcs_sha256")
		self.assertTrue(publication["candidate_cannot_start_any_attempt_before_success_terminal"])
		consumption = publication["land_recovery_authority_consumption"]
		self.assertEqual(consumption["accepted_authority_reference_schema"], "openapoc.land_recovery_authority_reference.v1")
		self.assertEqual(consumption["candidate_binding_field"], "land_recovery_authority_jcs_sha256")
		self.assertIn("durable_candidate-epoch-ledger-open-v1", consumption["consumption_linearization"])
		self.assertIn("exactly_one_candidate_epoch", consumption["single_consumer"])
		self.assertTrue(consumption["outcome_independent"])
		for surface in ("admission-intent", "attempt-started", "attempt-execution-began", "admitted", "land-exact-completed"):
			self.assertIn(surface, publication["success_terminal_required_downstream_binding_surfaces"])
		self.assertIn("impossible", publication["crash_recovery"]["partial_epoch_init_without_candidate"])
		self.assertIn("candidate-publication-failed", publication["crash_recovery"]["remote_ref_mismatched_preexisting_unauthorized_or_transport_source_authority_moved"])
		dispatch = ordinary["land_exact_construction_mode_dispatch"]
		self.assertTrue(dispatch["dispatch_precedes_remote_sha_tree_outcome_comparison"])
		self.assertTrue(dispatch["reviewed-source-merge"]["transport_and_candidate_sha_tree_must_differ"])
		self.assertTrue(dispatch["baseline-only-noop"]["transport_and_candidate_sha_tree_must_equal"])
		self.assertEqual(dispatch["baseline-only-noop"]["compound_transaction_id"], "baseline-only-noop-land-v1")
		self.assertEqual(dispatch["baseline-only-noop"]["pending_land_intent_or_reconciliation"], "forbidden")

	def test_preledger_oracle_sequence_forbids_optional_stopping(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		sequence = protocol["preledger_oracle_sequence"]
		self.assertEqual(sequence["ordered_phase_intent_kinds"], ["oracle-projection-stability", "oracle-calibration"])
		self.assertIn("one_global_preledger_sequence_operation", sequence["serialization"])
		self.assertEqual(sequence["parallel_duplicate_or_overlapping_phase_intents_for_same_subject_key"], "forbidden")
		self.assertEqual(sequence["second_intent_of_same_phase_for_same_subject_key"], "forbidden_even_after_terminal")
		self.assertIn("attempt-infrastructure-failed", sequence["same_intent_retry"])
		self.assertIn("before_attempt-execution-began", sequence["same_intent_retry"])
		self.assertIn("permanently_burn_subject_key", sequence["logical_failure_effect"])
		self.assertEqual(sequence["success_after_logical_failure_for_same_subject_key"], "invalid_preledger_log")
		self.assertIn("restarts_36_then_54", sequence["new_subject_after_logical_failure"])
		oracle = self.contracts["oracle_calibration"]
		self.assertIn("preledger_oracle_subject_key_jcs_sha256", oracle["projection_stability_probe"]["required_bindings"])
		self.assertIn("exhaustive_preledger_oracle_attempt_history_jcs_sha256", oracle["r1o_freeze_required_bindings"])
		self.assertEqual(
			set(oracle["r1o_freeze_required_bindings"]),
			set(sequence["r1b_epoch_init_import_required_bindings"]),
		)

	def test_live_admission_tip_is_derived_and_unique(self) -> None:
		live = self.contracts["ordinary_admission"]["candidate_parent_resolution"]["live_admission_resolution"]
		self.assertEqual(live["node_set"], "every_append_only_admitted_record")
		self.assertIn("no_transitive_eligible_admitted_descendant", live["live_set"])
		self.assertEqual(live["allowed_live_set_cardinality"], [0, 1])
		self.assertIn("derived_nonlive_parent", live["child_admission_effect"])
		self.assertIn("nearest_unique_unrevoked_ancestor_revives", live["descendant_revocation_effect"])
		self.assertIn("blocked", live["more_than_one_live_tip"])
		self.assertTrue(live["resolution_digest_is_input_to_candidate_parent_resolution"])

	def test_prospective_oracle_chain_binds_all_failure_and_invalidation_surfaces(self) -> None:
		surfaces = set(self.contracts["compatibility_delta"]["prospective_effective_oracle"]["required_binding_surfaces"])
		self.assertTrue(
			{
				"compatibility-delta-invalidated",
				"attempt-infrastructure-failed",
				"admission-failed",
			}.issubset(surfaces)
		)

	def test_oracle_campaign_counts_and_control_receipts_are_locked(self) -> None:
		oracle = self.contracts["oracle_calibration"]
		self.assertEqual(oracle["exact_key_count"], 18)
		self.assertEqual(oracle["exact_campaign_count"], 54)
		self.assertTrue(oracle["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(oracle["campaign_root_game_load_or_prior_save_input"], "forbidden")
		self.assertEqual(oracle["projection_stability_probe"]["exact_campaign_count"], 36)
		self.assertTrue(oracle["projection_stability_probe"]["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(
			oracle["projection_stability_probe"]["campaign_root_game_load_or_prior_save_input"],
			"forbidden",
		)
		self.assertEqual(len(oracle["projection_stability_probe"]["execution_profiles"]), 2)
		calibration_manifest = oracle["execution_cell_manifest"]
		self.assertEqual(calibration_manifest["canonical_replica_ordinal_order"], [0, 1, 2])
		self.assertEqual(calibration_manifest["exact_row_count"], oracle["exact_key_count"] * len(calibration_manifest["canonical_replica_ordinal_order"]))
		self.assertIn("cartesian_product", calibration_manifest["exact_row_set"])
		self.assertEqual(
			set(calibration_manifest["bijections_required"]),
			{
				"manifest_rows_to_initial_process_launch_rows",
				"manifest_rows_to_campaign_terminal_receipts",
				"manifest_rows_to_acceptance_projections",
			},
		)
		self.assertEqual(
			calibration_manifest["validation_sentinels"]["missing_duplicate_remapped_cross_phase_or_unmanifested_cell"],
			"reject_complete_attempt",
		)
		stability_manifest = oracle["projection_stability_probe"]["execution_cell_manifest"]
		self.assertEqual(stability_manifest["exact_row_count"], oracle["exact_key_count"] * len(oracle["projection_stability_probe"]["execution_profiles"]))
		self.assertIn("cartesian_product", stability_manifest["exact_row_set"])
		self.assertEqual(set(stability_manifest["bijections_required"]), set(calibration_manifest["bijections_required"]))
		self.assertEqual(
			stability_manifest["validation_sentinels"]["missing_duplicate_remapped_cross_phase_or_unmanifested_cell"],
			"reject_complete_attempt",
		)
		receipts = oracle["campaign_control_coverage_receipts"]
		self.assertEqual(receipts["authority_slice_id"], "R1c")
		self.assertEqual(set(receipts["raw_receipt_schemas"]), {"battle_turnbased", "save_reload"})
		self.assertEqual(set(receipts["every_full_game_campaign_requires_raw_categories"]), {"battle_turnbased", "save_reload"})
		self.assertIn("same_tested_sha_tree_intent_attempt_fence_campaign", receipts["bijection"])
		self.assertEqual(
			receipts["validation_sentinels"]["normalized_only_missing_duplicate_cross_attempt_cross_fence_or_cross_campaign_raw_receipt"],
			"logical_failure",
		)
		self.assertEqual(
			receipts["acceptance_projection_normalization"]["fields_exactly"],
			["obligation_id", "category", "result_class"],
		)
		required_control_bindings = {"campaign_control_raw_receipt_set_jcs_sha256", "campaign_control_receipt_bijection_jcs_sha256"}
		binding_surfaces = [
			set(oracle["projection_stability_probe"]["required_bindings"]),
			set(oracle["victory_oracle_required_bindings"]),
			set(self.contracts["ordinary_admission"]["tier_definitions"]["merge-3-v1"]["required_campaign_control_bindings"]),
			set(self.contracts["successor_release"]["required_bindings"]),
			set(self.contracts["nightly_schedule"]["manifest"]["required_bindings"]),
		]
		gates = {gate["id"]: gate for gate in self.slices["G"]["required_gates"]}
		binding_surfaces.extend((set(gates["activation-100-v1"]["required_bindings"]), set(gates["soak-24h-v1"]["required_bindings"])))
		for surface in binding_surfaces:
			self.assertTrue(required_control_bindings.issubset(surface))
		compatibility = self.contracts["compatibility_delta"]
		for prefix in ("parent", "delta_source"):
			self.assertIn(f"{prefix}_campaign_control_raw_receipt_set_jcs_sha256", compatibility["authorization_bindings"])
			self.assertIn(f"{prefix}_campaign_control_receipt_bijection_jcs_sha256", compatibility["authorization_bindings"])
		self.assertEqual(set(compatibility["parent_reference_audit"]["campaign_control_terminal_required_bindings"]), required_control_bindings)

	def test_three_twenty_and_one_hundred_campaign_gates_are_exact(self) -> None:
		ordinary = self.contracts["ordinary_admission"]
		merge = ordinary["tier_definitions"]["merge-3-v1"]
		self.assertEqual(merge["exact_campaign_count"], 3)
		self.assertEqual(merge["initial_state_per_campaign"], "new_game")
		self.assertTrue(merge["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(merge["campaign_root_game_load_or_prior_save_input"], "forbidden")
		self.assertTrue(merge["fresh_process_per_campaign"])
		self.assertTrue(merge["distinct_process_claim_pid_start_nonce_port_and_output_root_per_campaign"])
		self.assertTrue(merge["exactly_one_terminal_receipt_per_campaign"])
		self.assertTrue(merge["all_campaigns_victorious"])
		self.assertTrue(merge["all_campaigns_schema_and_recipe_coverage_required"])
		self.assertEqual(merge["partial_timeout_crash_forced_exit_prior_head_or_receipt_reuse"], "fail")
		self.assertEqual(self.contracts["successor_release"]["exact_campaign_count"], 20)
		self.assertTrue(self.contracts["successor_release"]["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(
			self.contracts["successor_release"]["campaign_root_game_load_or_prior_save_input"],
			"forbidden",
		)
		nightly = self.contracts["nightly_schedule"]["manifest"]
		self.assertEqual(nightly["exact_campaign_count"], 20)
		self.assertTrue(nightly["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(nightly["campaign_root_game_load_or_prior_save_input"], "forbidden")
		for template in nightly["phase_templates"]:
			self.assertEqual(template["key_count"] * template["profiles_per_key"], 20)
		gates = {gate["id"]: gate for gate in self.slices["G"]["required_gates"]}
		self.assertEqual(set(gates), {"desktop-presentation-v1", "desktop-performance-v1", "activation-100-v1", "soak-24h-v1"})
		activation = gates["activation-100-v1"]["predicate"]
		self.assertEqual(activation["exact_terminal_receipt_count"], 100)
		self.assertTrue(activation["full_new_game_to_classified_victory_per_execution_row"])
		self.assertEqual(activation["campaign_root_game_load_or_prior_save_input"], "forbidden")

	def test_nightly_ids_profiles_and_chain_freshness_are_canonical(self) -> None:
		nightly = self.contracts["nightly_schedule"]
		self.assertEqual(
			nightly["active_interval"]["night_ordinal_domain"],
			"canonical_uint64_rendered_as_exactly_20_decimal_digits",
		)
		self.assertIn("checked", nightly["active_interval"]["deadline_arithmetic"])
		self.assertIn("zero_based_profile_indexes", nightly["manifest"]["profile_index_canonical_flattening"])
		self.assertTrue(
			nightly["predicate"][
				"current_live_head_requires_a_successful_nightly_intent_after_its_latest_admission_or_effective_campaign_or_replay_chain_transition"
			]
		)
		cutoff = nightly["completion_cutoff_linearization"]
		self.assertIn("RFC3161_token_genTime", cutoff["normative_linearization_point"])
		self.assertIn("if_the_exact_bound_train-completed", cutoff["normative_linearization_point"])
		self.assertIn("not_local_rename", cutoff["active_interval_end_for_successful_completion"])
		self.assertIn("less_than_or_equal", cutoff["due_ordinal_range"])
		self.assertIn("schedule_remains_active", cutoff["crash_lock_loss_or_payload_change_before_durable_commit"])
		self.assertIn("lock_fence_ledger_head_next_ordinal", cutoff["post_token_revalidation"])

		period = nightly["active_interval"]["period_ns"]
		epoch = 1_000_000_000

		def due_count(signed_cutoff: int) -> int:
			return ((signed_cutoff - epoch) // period) + 1

		deadline_1 = epoch + period
		self.assertEqual(due_count(deadline_1 - 1), 1)
		self.assertEqual(due_count(deadline_1), 2)
		self.assertEqual(due_count(deadline_1 + 1), 2)
		append_time = deadline_1 + 1
		self.assertGreater(append_time, deadline_1)
		self.assertEqual(due_count(deadline_1 - 1), 1, "durable delayed append uses signed cutoff, not append time")
		self.assertIn("fresh_challenge", cutoff["crash_lock_loss_or_payload_change_before_durable_commit"])

	def test_replay_activation_precedes_same_landing_repeat(self) -> None:
		replay = self.contracts["replay_schema_upgrade"]
		repeat = self.contracts["successor_release"]
		self.assertTrue(replay["global_serialization"]["activation_must_precede_same_landing_integrated_repeat"])
		self.assertEqual(repeat["required_active_replay_schema_by_slice"], {"C": "control-epoch-v2", "F": "manual-step-v3"})
		self.assertTrue(repeat["intent_must_postdate_latest_effective_campaign_or_replay_chain_transition_for_subject"])
		for binding in ("intent_jcs_sha256", "attempt_started_jcs_sha256", "attempt_claim_jcs_sha256", "attempt_fence_jcs_sha256", "latest_effective_oracle_chain_transition_jcs_sha256"):
			self.assertIn(binding, repeat["required_bindings"])

	def test_postledger_serialization_matrix_is_closed_and_symmetric(self) -> None:
		protocol = self.contracts["batch_attempt_protocol"]
		serialization = protocol["global_serialized_operations"]
		self.assertEqual(serialization["lock"], "ledger_and_landing_lock")
		self.assertEqual(
			set(serialization["closed_operation_kinds"]),
			{
				"candidate-epoch-publication",
				"ordinary-admission",
				"land-exact",
				"land-exact-reconcile",
				"replay-schema-upgrade",
				"compatibility-delta",
				"parent-reference-audit",
				"integrated-repeat",
				"successor-branch-authorization",
				"nightly",
				"desktop-presentation",
				"desktop-performance",
				"activation",
				"soak",
				"baseline-or-lifecycle-transition",
				"train-terminal",
			},
		)
		self.assertIn("only_one_open_or_atomic_operation", serialization["mutual_exclusion"])
		transitions = serialization["allowed_same_operation_kind_transitions"]
		self.assertIn("land-abort-reconcile-open-v1", transitions["land-exact_to_land-exact-reconcile"])
		self.assertIn("train_abandonment", transitions["any_open_operation_to_train_terminal_branch"])
		self.assertEqual(serialization["all_other_operation_kind_transitions"], "forbidden")
		self.assertIn("after_soak_success_only_land_exact", serialization["G_gates_pending_state"])
		self.assertIn("after_land_abort_only_land_exact_reconcile", serialization["G_gates_pending_state"])
		self.assertIn("after_land_exact_reconcile_not_applied_only_fresh_land_exact", serialization["G_gates_pending_state"])
		ref = "/contracts/batch_attempt_protocol/global_serialized_operations"
		self.assertEqual(self.contracts["ordinary_admission"]["global_serialization_ref"], ref)
		self.assertEqual(self.contracts["replay_schema_upgrade"]["global_serialization"]["authority_ref"], ref)
		self.assertEqual(self.contracts["compatibility_delta"]["global_serialization_ref"], ref)
		self.assertEqual(self.contracts["compatibility_delta"]["parent_reference_audit"]["global_serialization_ref"], ref)
		self.assertEqual(self.contracts["successor_release"]["global_serialization"]["authority_ref"], ref)
		self.assertEqual(self.contracts["nightly_schedule"]["global_serialization"]["authority_ref"], ref)
		self.assertEqual(self.slices["G"]["admission_gate_land_order"]["global_serialization_ref"], ref)

	def test_repeat_and_nightly_never_share_evidence(self) -> None:
		repeat = self.contracts["successor_release"]
		nightly = self.contracts["nightly_schedule"]
		self.assertIn("forbidden", repeat["may_also_satisfy_due_nightly"])
		self.assertIn("forbidden", nightly["predicate"]["may_share_integrated_repeat_evidence"])
		self.assertTrue(nightly["global_serialization"]["nightly_and_integrated_repeat_are_always_distinct_attempts"])
		self.assertIn("non_recovery", repeat["must_complete_before"])
		self.assertIn("recovery", repeat["recovery_exception"])
		branch = repeat["successor_branch_authorization"]
		self.assertEqual(branch["record_type"], "successor-branch-authorized")
		self.assertTrue(branch["required_before_non_recovery_runtime_source_branch_creation"])
		self.assertTrue(branch["candidate_and_landing_must_cite_this_record"])
		self.assertTrue(branch["authorization_usable_only_while_bound_base_is_current_unique_live_landed_transport"])
		self.assertTrue(branch["source_head_must_descend_from_authorized_base_and_current_transport"])
		self.assertTrue(branch["source_pr_review_diff_base_must_equal_current_transport"])
		self.assertTrue(branch["current_live_landing_or_remote_transport_movement_stales_authorization_and_review"])

	def test_two_sibling_candidate_preserves_prior_landing(self) -> None:
		repeat = self.contracts["successor_release"]
		sentinel = repeat["two_sibling_accumulation_sentinel"]
		self.assertEqual(sentinel["test_id"], "two-sibling-candidate-preserves-prior-landing-v1")

		parents = {
			"C2": [],
			"D0": ["C2"],
			"D1_from_C2": ["C2"],
			"D1_onto_D0": ["D0"],
		}

		def is_ancestor(ancestor: str, descendant: str) -> bool:
			pending = list(parents[descendant])
			seen: set[str] = set()
			while pending:
				item = pending.pop()
				if item == ancestor:
					return True
				if item in seen:
					continue
				seen.add(item)
				pending.extend(parents[item])
			return False

		current_transport = "D0"
		self.assertFalse(is_ancestor(current_transport, "D1_from_C2"))
		self.assertIn("reject", sentinel["old_D1_source_after_D0_lands"])
		self.assertTrue(is_ancestor(current_transport, "D1_onto_D0"))
		self.assertEqual(set(sentinel["final_candidate_tree_must_contain_markers"]), {"C2", "D0", "D1"})

	def test_completion_repeat_set_is_recovery_safe(self) -> None:
		completion = self.scope["completion"]
		self.assertIn("required_integrated_repeat_landing_set_jcs_sha256", completion["required_bindings"])
		self.assertIn("required_integrated_repeat_completed_record_set_jcs_sha256", completion["required_bindings"])
		resolver = completion["predicate"]["required_integrated_repeat_landing_set"]
		self.assertIn("integrated_repeat_completed_release_digest", resolver["members"])
		self.assertIn("non_recovery", resolver["members"])
		self.assertIn("current_live_final_landing", resolver["members"])
		self.assertIn("never_successor_release_authority", resolver["recovery_candidate_or_landing"])
		self.assertIn("exclude", resolver["resolved_revoked_no_child_landings"])

	def test_completion_scopes_G_history_to_effective_subject(self) -> None:
		completion = self.scope["completion"]
		self.assertIn("effective_G_subject_admission_jcs_sha256", completion["required_bindings"])
		self.assertIn("effective_G_gate_success_record_set_jcs_sha256", completion["required_bindings"])
		predicate = completion["predicate"]
		resolver = predicate["effective_G_subject_resolver"]
		self.assertIn("current_live_final_admission", resolver["search_domain"])
		self.assertIn("unique_latest", resolver["selection"])
		self.assertIn("revoked", resolver["historical_G_failure"])
		self.assertEqual(predicate["exact_success_count_per_required_gate_for_effective_G_subject"], 1)
		self.assertEqual(predicate["required_gate_failure_or_open_count_for_effective_G_subject"], 0)
		self.assertTrue(predicate["historical_gate_records_never_count_toward_effective_subject_success_or_failure_cardinality"])

	def test_reconciliation_revalidates_authority_and_cannot_deadlock_G(self) -> None:
		reconciliation = self.contracts["ordinary_admission"]["land_exact_reconciliation"]
		for binding in (
			"original_land_exact_intent_jcs_sha256",
			"land_exact_aborted_jcs_sha256",
			"shared_operation_id",
			"pretransition_ledger_head_jcs_sha256",
			"candidate_parent_resolution_jcs_sha256",
			"candidate_ref_published_jcs_sha256",
		):
			self.assertIn(binding, reconciliation["reconcile_intent_required_bindings"])
		for required in (
			"active_scope_chain",
			"candidate_and_live_admission",
			"immutable_candidate_ref_and_ruleset",
			"source_pr_exact_head_and_state",
			"required_approvals",
			"authorized_actor_and_no_bypass_policy",
		):
			self.assertIn(required, reconciliation["full_authority_revalidation_required"])
		receipt = reconciliation["remote_update_receipt_required_when_remote_equals_intended"]
		self.assertTrue(receipt["old_sha_must_equal_pre_intent_transport"])
		self.assertTrue(receipt["new_sha_must_equal_exact_intended_candidate"])
		self.assertTrue(receipt["actor_must_equal_authorized_land_operator"])
		self.assertIn("receipt-authority-only", receipt["missing_or_nonconflictingly_ambiguous_provider_receipt_with_all_non_provider_authority_valid"])
		self.assertIn("reviewed-source-required", receipt["unauthorized_actor_ruleset_bypass_conflicting_provider_evidence_or_any_non_provider_authority_invalid"])
		self.assertEqual(
			reconciliation["outcomes"]["remote_equals_exact_intended_sha_tree_with_exact_authorized_update_receipt_and_full_authority"],
			"append_land_exact_reconciled",
		)
		self.assertIn(
			"receipt-authority-only",
			reconciliation["outcomes"]["remote_equals_exact_intended_sha_tree_with_only_missing_or_nonconflictingly_ambiguous_provider_receipt"],
		)
		self.assertIn(
			"reviewed-source-required",
			reconciliation["outcomes"]["remote_equals_exact_intended_sha_tree_with_unauthorized_actor_ruleset_bypass_conflicting_provider_evidence_or_any_other_authority_defect"],
		)
		self.assertIn("allow_new_land_exact_intent_only_after_full_current_authority_revalidation", reconciliation["outcomes"]["remote_equals_pre_intent_sha_tree"])
		self.assertEqual(reconciliation["failure_compound_transaction_id"], "land-reconcile-failure-revocation-recovery-authority-v1")

		recovery = self.contracts["ordinary_admission"]["land_reconciliation_failure_recovery"]
		self.assertEqual(
			recovery["ordered_logical_record_types"],
			["land-exact-reconcile-failed", "revoked", "land-recovery-authorized"],
		)
		for binding in (
			"land_exact_reconcile_intent_jcs_sha256",
			"failed_reconciliation_jcs_sha256",
			"revoked_subject_admission_jcs_sha256",
			"original_operation_id",
			"provider_evidence_query_receipt_set_jcs_sha256",
			"runtime_launch_template_jcs_sha256",
			"independent_reviewed_recovery_decision_jcs_sha256",
			"derived_remediates_set_jcs_sha256",
		):
			self.assertIn(binding, recovery["original_authority_record_required_bindings"])
		for derived in (
			"land_recovery_authority_jcs_sha256",
			"lineage_root_land_recovery_authorized_jcs_sha256",
			"authority_generation",
		):
			self.assertNotIn(derived, recovery["original_authority_record_required_bindings"])
		self.assertEqual(set(recovery["recovery_modes"]), {"receipt-authority-only", "reviewed-source-required"})
		self.assertTrue(recovery["single_use"])
		self.assertIn("first_candidate_epoch_publication", recovery["consumption_point"])
		self.assertIn("land-recovery-reauthorization-v1", recovery["later_candidate_after_consumption"])
		self.assertIn("all_four_gates_from_zero", recovery["receipt_authority_only_baseline_noop"])
		reference = recovery["land_recovery_authority_reference"]
		self.assertEqual(reference["schema"], "openapoc.land_recovery_authority_reference.v1")
		self.assertEqual(set(reference["closed_variants"]), {"original", "reauthorized"})
		self.assertEqual(reference["closed_variants"]["original"]["authority_generation"], 0)
		self.assertIn("never_inside", reference["authority_digest_location"])
		self.assertIn("after_the_complete_originating_logical_record_digest_exists", reference["authority_digest_rule"])
		self.assertEqual(
			set(reference["closed_variants"]["original"]["record_payload_forbidden_derived_fields"]),
			{"land_recovery_authority_jcs_sha256", "lineage_root_land_recovery_authorized_jcs_sha256", "authority_generation"},
		)
		self.assertEqual(
			reference["closed_variants"]["reauthorized"]["record_payload_forbidden_derived_fields"],
			["land_recovery_authority_jcs_sha256"],
		)
		self.assertIn("successor", reference["generation_rule"])
		self.assertIn("completed_original_logical_record_digest", reference["lineage_root_rule"])
		self.assertIn("predecessor_reference_lineage_root", reference["lineage_root_rule"])
		reauthorization = recovery["reauthorization"]
		self.assertEqual(reauthorization["compound_transaction_id"], "land-recovery-reauthorization-v1")
		self.assertEqual(reauthorization["record_type"], "land-recovery-reauthorized")
		self.assertEqual(
			set(reauthorization["required_predecessors"]),
			{
				"predecessor_land_recovery_authority_jcs_sha256",
				"predecessor_consuming_candidate_epoch_ledger_open_transaction_jcs_sha256",
				"failed_recovery_candidate_epoch_jcs_sha256",
			},
		)
		for binding in (
			"lineage_root_land_recovery_authorized_jcs_sha256",
			"authority_generation",
			"recovery_mode",
			"derived_remediates_set_jcs_sha256",
		):
			self.assertIn(binding, reauthorization["required_bindings"])
		self.assertNotIn("new_derived_remediates_set_jcs_sha256", reauthorization["required_bindings"])
		self.assertIn("ledger_derived_exact_union", reauthorization["derived_remediates_set_rule"])
		self.assertIn("never_caller_selected_or_narrowable", reauthorization["derived_remediates_set_rule"])
		terminal = reauthorization["failed_candidate_terminal_authority"]
		self.assertTrue(terminal["exactly_one_closed_variant_required"])
		self.assertEqual(
			set(terminal["closed_variants"]),
			{"candidate-publication-failed", "admission-failed", "revoked"},
		)
		self.assertTrue(terminal["terminal_must_bind_failed_recovery_candidate_epoch_and_consumed_authority"])
		self.assertTrue(reauthorization["receipt_authority_only_mode_may_survive_only_candidate_publication_failure_before_attempt_started_with_unchanged_sound_tree_and_zero_execution_or_robot_red"])
		self.assertTrue(reauthorization["admission_failure_revocation_execution_or_tree_change_forces_reviewed-source-required_mode"])
		self.assertTrue(reauthorization["reviewed_source_required_never_downgrades"])
		self.assertIn("arbitrary_finite", reauthorization["causal_chain"])
		self.assertIn("at_most_one_successor", reauthorization["lineage_rule"])
		self.assertTrue(reauthorization["reauthorization_record_must_precede_next_candidate_epoch_publication"])
		self.assertTrue(reauthorization["single_use_and_consumed_at_next_candidate_epoch_publication"])
		self.assertEqual(reauthorization["failed_candidate_or_prior_authority_receipt_attempt_gate_or_land_evidence_reuse"], "forbidden")
		self.assertEqual(
			recovery["lineage_validation_sentinels"],
			{
				"original_record_payload_contains_authority_digest_lineage_root_or_generation": "reject_circular_or_self_describing_original_authority",
				"original_failure_reauthorized_failure_reauthorized": "accept_one_linear_generation_zero_one_two_chain",
				"reviewed_reauthorization_set_differs_from_derived_remediates_set": "reject_before_publication",
				"two_successors_for_one_predecessor_and_failed_terminal": "reject_sibling_issuance",
				"two_candidates_consume_one_authority_digest": "reject_double_consumption",
				"reauthorized_receipt_authority_only_baseline_noop": "accept_when_live_unconsumed_sound_tree_and_zero_robot_red",
				"reviewed_source_required_to_receipt_authority_only": "reject_downgrade",
				"authority_generation_uint64_overflow": "reject_before_publication",
				"train_completion_with_live_unconsumed_authority": "reject_completion",
			},
		)

	def test_pending_land_recovery_cannot_accept_an_unauthorized_exact_sha(self) -> None:
		recovery = self.contracts["ordinary_admission"]["pending_land_intent_recovery"]
		self.assertTrue(recovery["same_operation_id_and_original_land_intent_required"])
		self.assertTrue(recovery["sha_tree_equality_alone_never_proves_authority_or_success"])
		for required in (
			"active_scope_chain",
			"candidate_and_live_admission",
			"candidate_parent_resolution",
			"no_revocation_or_unresolved_logical_red",
			"immutable_candidate_ref_and_ruleset",
			"source_head_and_transport_tuple",
			"source_pr_exact_head_and_state",
			"required_approvals",
			"required_checks_and_gate_digests",
			"authorized_actor_and_no_bypass_policy",
		):
			self.assertIn(required, recovery["full_authority_revalidation_required"])
		receipt = recovery["remote_update_receipt_required_when_remote_equals_intended"]
		self.assertTrue(receipt["old_sha_must_equal_pre_intent_transport"])
		self.assertTrue(receipt["new_sha_must_equal_exact_intended_candidate"])
		self.assertTrue(receipt["actor_must_equal_authorized_land_operator"])
		self.assertIn("land-exact-reconcile-failed", receipt["missing_ambiguous_or_unauthorized"])
		self.assertIn("full_authority_revalidation", recovery["outcomes"]["remote_equals_exact_intended_sha_tree"])
		self.assertEqual(recovery["applies_only_to_construction_mode"], "reviewed-source-merge")
		self.assertTrue(recovery["transport_and_candidate_sha_tree_must_differ"])
		self.assertEqual(recovery["abort_and_reconcile_open_compound_transaction_id"], "land-abort-reconcile-open-v1")
		self.assertEqual(recovery["mutex_release_between_abort_and_reconcile_intent"], "forbidden")
		self.assertEqual(recovery["completion_from_candidate_sha_tree_without_receipt_or_authority_revalidation"], "forbidden")

	def test_gates_bind_one_current_attempt_fence(self) -> None:
		for gate in self.slices["G"]["required_gates"]:
			self.assertEqual(gate["attempt_protocol_id"], COMMON_ATTEMPT_ID, gate["id"])
			self.assertTrue(gate["predicate"]["common_attempt_fence_validator_must_pass"], gate["id"])
			for binding in ("candidate_ref_jcs_sha256", "subject_admission_jcs_sha256", "intent_jcs_sha256", "attempt_started_jcs_sha256", "attempt_claim_jcs_sha256", "attempt_fence_jcs_sha256", "complete_terminal_receipt_set_jcs_sha256", "all_attempt_terminal_records_jcs_sha256"):
				self.assertIn(binding, gate["required_bindings"], gate["id"])

	def test_g_admission_gate_land_chain_is_exact(self) -> None:
		g = self.slices["G"]
		order = g["admission_gate_land_order"]
		self.assertTrue(order["ordinary_admission_must_succeed_before_first_gate_intent"])
		self.assertEqual(order["pending_record_type"], "g-gates-pending")
		self.assertEqual(order["admission_pending_compound_transaction_id"], "G-admission-pending-v1")
		self.assertTrue(order["pending_record_appended_in_same_atomic_transaction_as_G_admission_and_closed_only_by_G_land_or_revocation"])
		self.assertIn("every_gate_land_reconciliation_abandonment", order["g_pending_operation_id"])
		self.assertEqual(order["train_abandonment_dispatch_ref"], "/contracts/batch_attempt_protocol/train_abandonment/G_pending_dispatch")
		self.assertIn("admitted-subject-logical-red-revocation-v1", order["gate_failure"])
		self.assertEqual(
			order["gate_execution_order"],
			["desktop-presentation-v1", "desktop-performance-v1", "activation-100-v1", "soak-24h-v1"],
		)
		gates = {gate["id"]: gate for gate in g["required_gates"]}
		expected_predecessors = {
			"desktop-presentation-v1": "subject_admission_jcs_sha256",
			"desktop-performance-v1": "desktop_presentation_completed_jcs_sha256",
			"activation-100-v1": "desktop_performance_completed_jcs_sha256",
			"soak-24h-v1": "activation_completed_jcs_sha256",
		}
		for gate_id, predecessor in expected_predecessors.items():
			self.assertEqual(gates[gate_id]["required_predecessor_record_binding"], predecessor)
			self.assertIn(predecessor, gates[gate_id]["required_bindings"])
		for binding in order["success_chain_shared_bindings"]:
			for gate in gates.values():
				self.assertIn(binding, gate["required_bindings"], gate["id"])
		self.assertEqual(
			order["land_exact_required_bindings"],
			["subject_admission_jcs_sha256", "all_required_G_gate_success_records_jcs_sha256"],
		)
		self.assertIn("revoked", order["gate_failure"])
		recovery = order["recovery_generation_policy"]
		self.assertIn("G_gate_failure", recovery["trigger"])
		self.assertTrue(recovery["new_G_recovery_subject_must_first_complete_ordinary_admission"])
		self.assertTrue(recovery["all_four_gate_intents_restart_from_zero_in_canonical_order"])
		self.assertTrue(recovery["fresh_attempt_fence_manifest_and_evidence_required_per_gate"])
		self.assertEqual(recovery["historical_gate_success_receipt_attempt_manifest_or_cardinality_reuse"], "forbidden")
		self.assertTrue(recovery["land_exact_binds_only_the_new_G_recovery_subject_and_its_new_ordered_four_gate_success_set"])

	def test_soak_schedule_is_frozen_gap_free_and_nonselectable(self) -> None:
		gate = next(gate for gate in self.slices["G"]["required_gates"] if gate["id"] == "soak-24h-v1")
		for binding in (
			"soak_schedule_policy_jcs_sha256",
			"soak_workload_schedule_jcs_sha256",
			"activation_oracle_topology_jcs_sha256",
			"activation_100_extension_jcs_sha256",
			"process_launch_plan_authority_jcs_sha256",
				"planned_process_launch_prefix_policy_jcs_sha256",
				"soak_execution_window_jcs_sha256",
				"soak_campaign_root_start_receipt_set_jcs_sha256",
			"finalized_campaign_root_prefix_jcs_sha256",
			"finalized_campaign_root_prefix_count",
			"finalized_campaign_root_prefix_head_jcs_sha256",
			"finalized_process_launch_prefix_jcs_sha256",
			"finalized_process_launch_prefix_count",
			"finalized_process_launch_prefix_head_jcs_sha256",
		):
			self.assertIn(binding, gate["required_bindings"])
		policy = gate["predicate"]["schedule_policy"]
		self.assertEqual(policy["authority"], "admitted_F_gate_spec_frozen_before_G_candidate")
		self.assertEqual(policy["candidate_subject_runtime_input_or_oracle_bindings"], "forbidden")
		schedule = gate["predicate"]["workload_schedule"]
		self.assertIn("G_soak_intent_materialized_after_activation_success", schedule["authority"])
		self.assertIn("attempt_independent", schedule["authority"])
		self.assertEqual(schedule["policy_digest_field"], "soak_schedule_policy_jcs_sha256")
		self.assertIn("soak_schedule_policy_jcs_sha256", schedule["digest_inputs"])
		for candidate_binding in (
			"runtime_launch_template_jcs_sha256",
			"input_closure_digest",
			"active_effective_campaign_oracle_chain_jcs_sha256",
			"active_effective_replay_oracle_chain_jcs_sha256",
		):
			self.assertNotIn(candidate_binding, policy["fixed_fields"])
			self.assertIn(candidate_binding, schedule["digest_inputs"])
		for attempt_specific_binding in (
			"process_launch_plan_authority_jcs_sha256",
			"planned_process_launch_prefix_policy_jcs_sha256",
			"attempt_origin_monotonic_ns",
			"launch_cutoff_monotonic_ns",
		):
			self.assertNotIn(attempt_specific_binding, schedule["digest_inputs"])
		self.assertIn("gap_free_prefix", schedule["campaign_root_ordinals"])
		self.assertIn("gap_free_prefix", schedule["process_launch_ordinals"])
		self.assertIn("campaign_terminal_receipts_each_biject_campaign_prefix", schedule["receipt_bijections"])
		self.assertIn("realized_process_receipts_biject_process_prefix", schedule["receipt_bijections"])
		self.assertIn("root_start_receipts", schedule["receipt_bijections"])
		self.assertTrue(schedule["full_new_game_to_classified_victory_per_campaign"])
		self.assertEqual(schedule["campaign_root_game_load_or_prior_save_input"], "forbidden")
		self.assertEqual(schedule["row_index_formula"], "campaign_ordinal_mod_100")
		self.assertEqual(schedule["minimum_complete_topology_cycles"], 1)
		self.assertIn("at_least_100", schedule["minimum_completed_campaigns"])
		self.assertTrue(schedule["unique_campaign_claim_per_campaign_ordinal_and_unique_process_claim_port_and_output_root_per_process_launch_ordinal"])
		self.assertIn("no_new_campaign_root", schedule["launch_cutoff"])
		self.assertIn("cutoff_minus_1ns", schedule["root_cutoff_boundaries"])
		self.assertIn("pid_start_identity", schedule["root_start_receipt_authority"])
		self.assertIn("execution_window_clock_domain", schedule["root_start_receipt_authority"])
		self.assertIn("cannot_spawn_during_drain", schedule["root_plan_start_atomicity"])
		self.assertIn("root_start_receipt", schedule["drain_deadline"])
		self.assertIn("continuations", schedule["drain_process_rule"])
		self.assertIn("soak-failed_logical_red_with_no_retry", schedule["coordinator_interruption"])
		self.assertEqual(schedule["operator_supplied_row_recipe_seed_or_profile"], "forbidden")
		sentinels = gate["predicate"]["execution_window_validation_sentinels"]
		self.assertIn("distinct_fence_window_and_prefix_digests", sentinels["same_intent_two_pre_execution_attempts"])
		self.assertEqual(sentinels["post_execution_interruption_then_retry"], "reject_retry")
		self.assertEqual(sentinels["root_start_at_cutoff_minus_1ns"], "accept_root")
		self.assertEqual(sentinels["root_plan_or_start_at_cutoff"], "logical_failure")
		self.assertEqual(sentinels["root_start_receipt_missing_pid_identity_or_cross_clock_timestamp"], "logical_failure")
		self.assertEqual(sentinels["drain_deadline_not_exact_checked_add_from_bound_root_start_receipt"], "logical_failure")
		self.assertIn("without_new_campaign_ordinal", sentinels["continuation_for_pre_cutoff_root_during_bounded_drain"])
		self.assertTrue(gate["predicate"]["minimum_completed_campaigns_must_be_at_least_100_and_divisible_by_100"])

	def test_r1c_r1p_launcher_ownership_and_continuation_closure(self) -> None:
		controls = self.contracts["oracle_calibration"]["campaign_control_coverage_receipts"]
		ownership = controls["implementation_ownership"]
		self.assertIn("state_machine", ownership["R1c"])
		self.assertIn("subprocess_launch", ownership["R1p"])
		self.assertEqual(ownership["R1c_direct_subprocess_or_runtime_closure_implementation"], "forbidden")
		receipt = controls["raw_receipt_schemas"]["save_reload"]
		for binding in (
			"pre_process_claim_jcs_sha256",
			"post_process_claim_jcs_sha256",
			"pre_process_pid_start_identity_jcs_sha256",
			"post_process_pid_start_identity_jcs_sha256",
			"pre_runtime_image_receipt_jcs_sha256",
			"post_runtime_image_receipt_jcs_sha256",
			"save_production_receipt_jcs_sha256",
			"continuation_input_closure_jcs_sha256",
			"orderly_wait_reap_receipt_jcs_sha256",
		):
			self.assertIn(binding, receipt["required_bindings"])
		self.assertTrue(receipt["predicate"]["orderly_wait_and_reap_required"])
		self.assertEqual(receipt["predicate"]["terminate_kill_or_escalation_count"], 0)
		self.assertTrue(receipt["predicate"]["continuation_input_closure_is_attempt_local_read_only_and_same_fence"])
		self.assertEqual(
			receipt["predicate"]["allowed_relaunch_delta_fields_exactly"],
			["Game.Load", "Framework.Harness.Port", "Game.Save.Directory", "process_nonce_transport"],
		)
		self.assertTrue(receipt["predicate"]["all_other_normalized_launch_argv_environment_and_input_fields_byte_equal"])

	def test_performance_predicates_apply_per_run_and_aggregate(self) -> None:
		gate = next(gate for gate in self.slices["G"]["required_gates"] if gate["id"] == "desktop-performance-v1")
		predicate = gate["predicate"]
		self.assertEqual(predicate["independent_run_count"], 3)
		self.assertEqual(predicate["exact_samples_per_class_per_run"], 10_000)
		self.assertEqual(predicate["exact_retained_samples_per_class"], 30_000)
		self.assertEqual(
			predicate["all_percentile_tail_hard_ceiling_zero_count_and_turbo_ratio_predicates_apply_to"],
			["each_independent_run_per_class", "aggregate_all_three_runs_per_class"],
		)
		contamination = predicate["host_contamination_policy"]
		self.assertEqual(contamination["retry_after_execution_began"], "forbidden")
		self.assertIn("desktop-performance-failed", contamination["after_attempt_execution_began"])

	def test_planning_validators_run_in_harness_ci(self) -> None:
		workflow_bytes = HARNESS_WORKFLOW_PATH.read_bytes()
		self.assertEqual(
			hashlib.sha256(workflow_bytes).hexdigest(),
			"b77af6452fc3a12f37ba6db370b92d37ecb2d48cbf1f8e4558907ec0b7a8f66c",
			"P0 freezes the regular unprivileged workflow independently of P",
		)
		workflow = workflow_bytes.decode("utf-8")
		for command in (
			"python3 tools/test_capture_timing_sources.py",
			"python3 tools/capture_timing_sources.py verify-offline",
			"python3 tools/test_timing_train_scope.py",
			"python3 tools/test_timing_source_disposition.py",
			"python3 tools/test_pr_p_scope.py",
		):
			self.assertEqual(workflow.count(command), 1)
		self.assertLess(
			workflow.index("python3 tools/test_pr_p_scope.py"),
			workflow.index("python3 tools/test_capture_timing_sources.py"),
			"the scope firewall must run before any other changed planning Python",
		)
		for contract_fragment in (
			"permissions:\n  contents: read",
			"uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1",
			"persist-credentials: false",
			"ref: ${{ github.event.pull_request.head.sha }}",
			"fetch-depth: 0",
			"github.event.pull_request.base.repo.node_id == 'R_kgDOUBJ-Dg'",
			"github.event.pull_request.head.repo.node_id == 'R_kgDOUBJ-Dg'",
			"github.event.pull_request.base.ref == 'develop'",
			"github.event.pull_request.head.ref == 'khallmark/render-simulation-design'",
			"github.event.pull_request.base.sha",
			'--base "$EVENT_BASE_SHA" --head "$EVENT_HEAD_SHA"',
			"hashFiles('tools/test_timing_train_scope.py') != ''",
		):
			self.assertIn(contract_fragment, workflow)
		for forbidden in (
			"manifest_base=",
			"descendant validation skipped",
			"$'A\\tdocs/timing/pr-p-scope-v1.json'",
		):
			self.assertNotIn(forbidden, workflow)
		self.assertNotIn("pull_request_target", workflow)


if __name__ == "__main__":
	unittest.main()
