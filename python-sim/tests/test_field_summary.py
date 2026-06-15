import csv
from io import StringIO

from field_summary import (
    collect_summaries,
    grouped_summary_rows,
    main,
    parse_evidence_summary,
    unique_accepted_frame_rows,
    write_accepted_frames,
    write_summaries,
)


def evidence_text(csv_row: str) -> str:
    return (
        "HFText evidencia RX\n"
        "Gerado em: 2026-06-06T17:59:11\n"
        "\n"
        "--- Resumo CSV ---\n"
        "generated_at,callsign,symbol_duration_s,f0_hz,f1_hz,amplitude,preamble_bits,detailed_log,"
        "rx_elapsed_s,rx_accepted,rx_rejected_strong,rx_phys_length,rx_sync,rx_quality,"
        "received_text,accepted_length\n"
        f"{csv_row}\n"
        "\n"
        "--- Quadros aceitos CSV ---\n"
        "accepted_at,rx_elapsed_s,modulation,symbol_duration_s,sample_rate_hz,"
        "f0_hz,f1_hz,amplitude,preamble_bits,length,quality,offset_samples,offsets_tried,text\n"
        '"2026-06-06T17:59:10",120.0,"2-FSK v0.1",0.300,48000,1200.0,1600.0,'
        '0.80,64,13,"68.8%",0,20,"pu5lrk Ola!"\n'
        "\n"
        "--- Texto recebido ---\n"
        "pu5lrk Ola!\n"
    )


def test_parse_evidence_summary_reads_csv_block(tmp_path):
    path = tmp_path / "HFText-rx-evidence.txt"
    path.write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )

    summary = parse_evidence_summary(path)

    assert summary is not None
    assert summary.source_path == path
    assert summary.row["callsign"] == "pu5lrk"
    assert summary.row["rx_accepted"] == "1"
    assert summary.row["rx_quality"] == "68.8%"
    assert summary.row["received_text"] == "pu5lrk Ola!"
    assert summary.frame_rows[0]["modulation"] == "2-FSK v0.1"
    assert summary.frame_rows[0]["text"] == "pu5lrk Ola!"


def test_parse_evidence_summary_handles_multiline_received_text(tmp_path):
    path = tmp_path / "HFText-rx-evidence.txt"
    path.write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,2,0,4,4,"72.0%","pu5lrk Ola!\npu5lrk Teste",13'),
        encoding="utf-8",
    )

    summary = parse_evidence_summary(path)

    assert summary is not None
    assert summary.row["received_text"] == "pu5lrk Ola!\npu5lrk Teste"


def test_collect_summaries_skips_txt_without_csv_block(tmp_path):
    (tmp_path / "empty.txt").write_text("sem resumo\n", encoding="utf-8")
    (tmp_path / "valid.txt").write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )

    summaries = collect_summaries(tmp_path)

    assert len(summaries) == 1
    assert summaries[0].source_path.name == "valid.txt"


def test_write_summaries_adds_source_column(tmp_path):
    path = tmp_path / "valid.txt"
    path.write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    summaries = collect_summaries(tmp_path)
    output = StringIO()

    write_summaries(output, summaries)

    rows = list(csv.DictReader(StringIO(output.getvalue())))
    assert rows[0]["source_txt"].endswith("valid.txt")
    assert rows[0]["accepted_length"] == "13"


def test_write_accepted_frames_adds_source_column(tmp_path):
    path = tmp_path / "valid.txt"
    path.write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    summaries = collect_summaries(tmp_path)
    output = StringIO()

    write_accepted_frames(output, summaries)

    rows = list(csv.DictReader(StringIO(output.getvalue())))
    assert rows[0]["source_txt"].endswith("valid.txt")
    assert rows[0]["sample_rate_hz"] == "48000"
    assert rows[0]["text"] == "pu5lrk Ola!"


def test_write_accepted_frames_deduplicates_cumulative_evidence(tmp_path):
    first = tmp_path / "one.txt"
    second = tmp_path / "two.txt"
    first.write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    second.write_text(
        evidence_text('"2026-06-06T18:00:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,180.0,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    summaries = collect_summaries(tmp_path)

    output = StringIO()
    write_accepted_frames(output, summaries)
    unique_rows = list(csv.DictReader(StringIO(output.getvalue())))

    raw_output = StringIO()
    write_accepted_frames(raw_output, summaries, keep_duplicates=True)
    raw_rows = list(csv.DictReader(StringIO(raw_output.getvalue())))

    assert len(unique_accepted_frame_rows(summaries)) == 1
    assert len(unique_rows) == 1
    assert len(raw_rows) == 2


def test_grouped_summary_rows_aggregates_by_selected_columns(tmp_path):
    (tmp_path / "one.txt").write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,120.0,1,0,4,4,"68.0%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    (tmp_path / "two.txt").write_text(
        evidence_text('"2026-06-06T18:00:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,100.0,0,2,5,6,"50.0%","",0'),
        encoding="utf-8",
    )

    rows = grouped_summary_rows(collect_summaries(tmp_path), ["symbol_duration_s", "amplitude"])

    assert rows == [
        {
            "symbol_duration_s": "0.300",
            "amplitude": "0.80",
            "evidences": "2",
            "rx_accepted": "1",
            "accept_rate": "0.500",
            "avg_quality_pct": "59.0",
            "min_quality_pct": "50.0",
            "avg_rx_elapsed_s": "110.00",
            "avg_rejected_strong": "1.00",
            "avg_rx_phys_length": "4.50",
            "avg_rx_sync": "5.00",
        }
    ]


def test_main_writes_aggregate_csv(tmp_path, capsys):
    input_dir = tmp_path / "logs"
    input_dir.mkdir()
    (input_dir / "valid.txt").write_text(
        evidence_text('"2026-06-06T17:59:11","pu5lrk",0.300,1200.0,1600.0,0.80,64,0,121.9,1,0,4,4,"68.8%","pu5lrk Ola!",13'),
        encoding="utf-8",
    )
    output_path = tmp_path / "field_summary.csv"

    code = main(["--input-dir", str(input_dir), "--output", str(output_path)])
    output = capsys.readouterr().out

    assert code == 0
    assert output_path.exists()
    assert (input_dir / "field_summary_groups.csv").exists()
    assert (input_dir / "field_frames.csv").exists()
    assert "evidencias,1" in output
    assert "quadros_aceitos,1" in output
    assert "quadros_aceitos_unicos,1" in output
    assert "qualidade_media_pct,68.8" in output
    assert "groups_csv" in output
    assert "frames_csv" in output
