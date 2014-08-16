package de.egh.odo.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.List;

import de.egh.dynamodrivenodometer.MessageBox;

public class Test {

	@org.junit.Test
	public void testToString() {

		MessageBox box = new MessageBox(null);
		MessageBox box2;

		box.add("msg1");

		momento();
		box.add("m s g 2");

		momento();
		box.add("msg 3");

		String s = box.asString();
		box = null;

		box2 = new MessageBox(s);

		momento();
		assertTrue("Missing entry",
				box2.getSorted().get(0).getText().equals("msg 3"));
		assertTrue("Timestamp to high",
				box2.getSorted().get(0).getTimestamp() < System
						.currentTimeMillis());
		assertTrue("Timestamp 0", box2.getSorted().get(0).getTimestamp() > 0);

	}

	@org.junit.Test
	public void testToStringInvalids() {

		MessageBox box = new MessageBox(null);
		MessageBox box2;

		String s1 = box.asString();
		box2 = new MessageBox(s1);
		assertTrue("Box not empty", box2.getSorted().size() == 0);

		momento();
		box.add("");

		String s2 = box.asString();
		box2 = new MessageBox(s2);
		assertTrue("Empty message not filtered", box2.getSorted().size() == 0);

	}

	@org.junit.Test
	public void testDuplicates() {

		MessageBox box = new MessageBox(null);

		momento();
		box.add("msg1");

		momento();
		box.add("msg2");

		momento();
		box.add("msg3");

		momento();
		box.add("msg2");

		assertTrue("3 Entries expected", box.getSorted().size() == 3);

	}

	@org.junit.Test
	public void testDropout() {

		MessageBox box = new MessageBox(null);

		box.add("msg1");

		momento();
		box.add("msg2");

		momento();
		box.add("msg3");

		momento();
		box.add("msg4");

		momento();
		box.add("msg5");

		momento();
		box.add("msg6");

		momento();
		box.add("msg7");

		assertTrue("Where is the entry?", box.getSorted().get(0).getText()
				.equals("msg7"));

		assertTrue("Where is the entry?", box.getSorted().get(1).getText()
				.equals("msg6"));

		assertTrue("Where is the entry?", box.getSorted().get(2).getText()
				.equals("msg5"));

	}

	@org.junit.Test
	public void test() {

		List<MessageBox.Msg> eList = new ArrayList<MessageBox.Msg>();
		MessageBox box = new MessageBox(null);

		assertEquals("Not empty", 0, box.getSorted().size());

		box.add("msg1");

		assertEquals("msg1", box.getSorted().get(0).getText());

		long eTimestamp1Low = System.currentTimeMillis();
		assertTrue("msg1: timestamp to low "
				+ box.getSorted().get(0).getTimestamp() + " " + eTimestamp1Low,
				box.getSorted().get(0).getTimestamp() >= eTimestamp1Low);
		assertTrue("msg1: timestamp not high", box.getSorted().get(0)
				.getTimestamp() <= System.currentTimeMillis());

		// We need this for a later test
		long msg1FirstTimestamp = box.getSorted().get(0).getTimestamp();

		// We need a second timestamp
		momento();
		long eTimestamp2Low = System.currentTimeMillis();

		box.add("msg2");
		assertEquals("msg2", box.getSorted().get(0).getText());

		assertTrue("msg2: timestamp to low", box.getSorted().get(0)
				.getTimestamp() >= eTimestamp2Low);
		assertTrue("msg2: timestamp not high", box.getSorted().get(0)
				.getTimestamp() <= System.currentTimeMillis());
		assertEquals("To much entries", 2, box.getSorted().size());

		assertTrue("Ordering failure",
				box.getSorted().get(0).getText().equals("msg2"));

		momento();
		// Add first message again, check the timestamp.
		box.add("msg1");
		assertTrue("Ordering failure",
				box.getSorted().get(0).getText().equals("msg1"));

		momento();
		// Same with second, must be now at index 0
		box.add("msg2");
		assertTrue("Ordering failure",
				box.getSorted().get(0).getText().equals("msg2"));

		momento();
		// Full box
		box.add("msg3");
		assertEquals("There must be three entries now", 3, box.getSorted()
				.size());
		assertTrue("Ordering failure",
				box.getSorted().get(0).getText().equals("msg3"));

	}

	/** Wait for next ms. */
	private void momento() {

		long now = System.currentTimeMillis();
		while (System.currentTimeMillis() <= now)
			;

	}
}
